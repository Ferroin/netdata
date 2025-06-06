// SPDX-License-Identifier: GPL-3.0-or-later

package jobmgr

import (
	"context"
	"fmt"
	"io"
	"log/slog"
	"os"
	"slices"
	"strings"
	"sync"
	"time"

	"github.com/netdata/netdata/go/plugins/logger"
	"github.com/netdata/netdata/go/plugins/pkg/netdataapi"
	"github.com/netdata/netdata/go/plugins/pkg/safewriter"
	"github.com/netdata/netdata/go/plugins/pkg/ticker"
	"github.com/netdata/netdata/go/plugins/plugin/go.d/agent/confgroup"
	"github.com/netdata/netdata/go/plugins/plugin/go.d/agent/functions"
	"github.com/netdata/netdata/go/plugins/plugin/go.d/agent/module"
	"github.com/netdata/netdata/go/plugins/plugin/go.d/agent/vnodes"

	"github.com/mattn/go-isatty"
	"gopkg.in/yaml.v2"
)

var isTerminal = isatty.IsTerminal(os.Stdout.Fd()) || isatty.IsTerminal(os.Stdin.Fd())

func New() *Manager {
	mgr := &Manager{
		Logger: logger.New().With(
			slog.String("component", "job manager"),
		),
		Out:   io.Discard,
		FnReg: noop{},

		Vnodes: make(map[string]*vnodes.VirtualNode),

		discoveredConfigs: newDiscoveredConfigsCache(),
		seenConfigs:       newSeenConfigCache(),
		exposedConfigs:    newExposedConfigCache(),
		runningJobs:       newRunningJobsCache(),
		retryingTasks:     newRetryingTasksCache(),

		started:  make(chan struct{}),
		api:      netdataapi.New(safewriter.Stdout),
		addCh:    make(chan confgroup.Config),
		rmCh:     make(chan confgroup.Config),
		dyncfgCh: make(chan functions.Function),
	}

	return mgr
}

type Manager struct {
	*logger.Logger

	PluginName     string
	Out            io.Writer
	Modules        module.Registry
	RunJob         []string
	ConfigDefaults confgroup.Registry
	VarLibDir      string
	FnReg          FunctionRegistry
	Vnodes         map[string]*vnodes.VirtualNode

	fileStatus *fileStatus

	discoveredConfigs *discoveredConfigs
	seenConfigs       *seenConfigs
	exposedConfigs    *exposedConfigs
	retryingTasks     *retryingTasks
	runningJobs       *runningJobs

	ctx      context.Context
	started  chan struct{}
	api      dyncfgAPI
	addCh    chan confgroup.Config
	rmCh     chan confgroup.Config
	dyncfgCh chan functions.Function

	waitCfgOnOff string // block processing of discovered configs until "enable"/"disable" is received from Netdata
}

func (m *Manager) Run(ctx context.Context, in chan []*confgroup.Group) {
	m.Info("instance is started")
	defer func() { m.cleanup(); m.Info("instance is stopped") }()
	m.ctx = ctx

	m.FnReg.Register("config", m.dyncfgConfig)

	m.dyncfgVnodeModuleCreate()

	for _, cfg := range m.Vnodes {
		m.dyncfgVnodeJobCreate(cfg, dyncfgRunning)
	}

	for name := range m.Modules {
		m.dyncfgCollectorModuleCreate(name)
	}

	m.loadFileStatus()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() { defer wg.Done(); m.runFileStatusPersistence() }()

	wg.Add(1)
	go func() { defer wg.Done(); m.runProcessConfGroups(in) }()

	wg.Add(1)
	go func() { defer wg.Done(); m.run() }()

	wg.Add(1)
	go func() { defer wg.Done(); m.runNotifyRunningJobs() }()

	close(m.started)

	wg.Wait()
	<-m.ctx.Done()
}

func (m *Manager) runProcessConfGroups(in chan []*confgroup.Group) {
	for {
		select {
		case <-m.ctx.Done():
			return
		case groups := <-in:
			for _, gr := range groups {
				a, r := m.discoveredConfigs.add(gr)
				m.Debugf("received configs: %d/+%d/-%d ('%s')", len(gr.Configs), len(a), len(r), gr.Source)
				if len(m.RunJob) > 0 {
					a = slices.DeleteFunc(a, func(config confgroup.Config) bool {
						return !slices.ContainsFunc(m.RunJob, func(name string) bool { return config.Name() == name })
					})
				}
				sendConfigs(m.ctx, m.rmCh, r...)
				sendConfigs(m.ctx, m.addCh, a...)
			}
		}
	}
}

func (m *Manager) run() {
	for {
		if m.waitCfgOnOff != "" {
			select {
			case <-m.ctx.Done():
				return
			case fn := <-m.dyncfgCh:
				m.dyncfgCollectorSeqExec(fn)
			}
		} else {
			select {
			case <-m.ctx.Done():
				return
			case cfg := <-m.addCh:
				m.addConfig(cfg)
			case cfg := <-m.rmCh:
				m.removeConfig(cfg)
			case fn := <-m.dyncfgCh:
				switch id := fn.Args[0]; true {
				case strings.HasPrefix(id, dyncfgCollectorIDPrefix):
					m.dyncfgCollectorSeqExec(fn)
				case strings.HasPrefix(id, dyncfgVnodeID):
					m.dyncfgVnodeSeqExec(fn)
				default:
					m.dyncfgRespf(fn, 503, "unknown function '%s' (%s).", fn.Name, id)
				}
			}
		}
	}
}

func (m *Manager) addConfig(cfg confgroup.Config) {
	if _, ok := m.Modules.Lookup(cfg.Module()); !ok {
		return
	}

	m.retryingTasks.remove(cfg)

	scfg, ok := m.seenConfigs.lookup(cfg)
	if !ok {
		scfg = &seenConfig{cfg: cfg}
		m.seenConfigs.add(scfg)
	}

	ecfg, ok := m.exposedConfigs.lookup(cfg)
	if !ok {
		scfg.status = dyncfgAccepted
		ecfg = scfg
		m.exposedConfigs.add(ecfg)
	} else {
		sp, ep := scfg.cfg.SourceTypePriority(), ecfg.cfg.SourceTypePriority()
		if ep > sp || (ep == sp && ecfg.status == dyncfgRunning) {
			return
		}
		if ecfg.status == dyncfgRunning {
			m.stopRunningJob(ecfg.cfg.FullName())
			m.fileStatus.remove(ecfg.cfg)
		}
		scfg.status = dyncfgAccepted
		m.exposedConfigs.add(scfg) // replace existing exposed
		ecfg = scfg
	}

	m.dyncfgCollectorJobCreate(ecfg.cfg, ecfg.status)

	if isTerminal || m.PluginName == "nodyncfg" { // FIXME: quick fix of TestAgent_Run (agent_test.go)
		m.dyncfgConfigEnable(functions.Function{Args: []string{dyncfgJobID(ecfg.cfg), "enable"}})
	} else {
		m.waitCfgOnOff = ecfg.cfg.FullName()
	}
}

func (m *Manager) removeConfig(cfg confgroup.Config) {
	m.retryingTasks.remove(cfg)

	scfg, ok := m.seenConfigs.lookup(cfg)
	if !ok {
		return
	}
	m.seenConfigs.remove(cfg)

	ecfg, ok := m.exposedConfigs.lookup(cfg)
	if !ok || scfg.cfg.UID() != ecfg.cfg.UID() {
		return
	}

	m.exposedConfigs.remove(cfg)
	m.stopRunningJob(cfg.FullName())
	m.fileStatus.remove(cfg)

	if !isStock(cfg) || ecfg.status == dyncfgRunning {
		m.dyncfgJobRemove(cfg)
	}
}

func (m *Manager) runNotifyRunningJobs() {
	tk := ticker.New(time.Second)
	defer tk.Stop()

	for {
		select {
		case <-m.ctx.Done():
			return
		case clock := <-tk.C:
			m.runningJobs.lock()
			m.runningJobs.forEach(func(_ string, job *module.Job) { job.Tick(clock) })
			m.runningJobs.unlock()
		}
	}
}

func (m *Manager) startRunningJob(job *module.Job) {
	m.runningJobs.lock()
	defer m.runningJobs.unlock()

	if job, ok := m.runningJobs.lookup(job.FullName()); ok {
		job.Stop()
	}

	go job.Start()
	m.runningJobs.add(job.FullName(), job)
}

func (m *Manager) stopRunningJob(name string) {
	m.runningJobs.lock()
	defer m.runningJobs.unlock()

	if job, ok := m.runningJobs.lookup(name); ok {
		job.Stop()
		m.runningJobs.remove(name)
	}
}

func (m *Manager) cleanup() {
	m.FnReg.Unregister("config")

	m.runningJobs.lock()
	defer m.runningJobs.unlock()

	m.runningJobs.forEach(func(key string, job *module.Job) {
		job.Stop()
	})
}

func (m *Manager) createCollectorJob(cfg confgroup.Config) (*module.Job, error) {
	creator, ok := m.Modules[cfg.Module()]
	if !ok {
		return nil, fmt.Errorf("can not find %s module", cfg.Module())
	}

	var vnode *vnodes.VirtualNode

	if cfg.Vnode() != "" {
		n, ok := m.Vnodes[cfg.Vnode()]
		if !ok || n == nil {
			return nil, fmt.Errorf("vnode '%s' is not found", cfg.Vnode())
		}
		vnode = n
	}

	m.Debugf("creating %s[%s] job, config: %v", cfg.Module(), cfg.Name(), cfg)

	mod := creator.Create()

	if err := applyConfig(cfg, mod); err != nil {
		return nil, err
	}

	jobCfg := module.JobConfig{
		PluginName:      m.PluginName,
		Name:            cfg.Name(),
		ModuleName:      cfg.Module(),
		FullName:        cfg.FullName(),
		UpdateEvery:     cfg.UpdateEvery(),
		AutoDetectEvery: cfg.AutoDetectionRetry(),
		Priority:        cfg.Priority(),
		Labels:          makeLabels(cfg),
		IsStock:         cfg.SourceType() == "stock",
		Module:          mod,
		Out:             m.Out,
	}
	if vnode != nil {
		jobCfg.Vnode = *vnode.Copy()
	}

	job := module.NewJob(jobCfg)

	return job, nil
}

func runRetryTask(ctx context.Context, out chan<- confgroup.Config, cfg confgroup.Config) {
	t := time.NewTimer(time.Second * time.Duration(cfg.AutoDetectionRetry()))
	defer t.Stop()

	select {
	case <-ctx.Done():
	case <-t.C:
		sendConfigs(ctx, out, cfg)
	}
}

func sendConfigs(ctx context.Context, out chan<- confgroup.Config, cfgs ...confgroup.Config) {
	for _, cfg := range cfgs {
		select {
		case <-ctx.Done():
			return
		case out <- cfg:
		}
	}
}

func isStock(cfg confgroup.Config) bool {
	return cfg.SourceType() == confgroup.TypeStock
}

func isDyncfg(cfg confgroup.Config) bool {
	return cfg.SourceType() == confgroup.TypeDyncfg
}

func applyConfig(cfg confgroup.Config, module any) error {
	bs, err := yaml.Marshal(cfg)
	if err != nil {
		return err
	}
	return yaml.Unmarshal(bs, module)
}

func makeLabels(cfg confgroup.Config) map[string]string {
	labels := make(map[string]string)
	for name, value := range cfg.Labels() {
		n, ok1 := name.(string)
		v, ok2 := value.(string)
		if ok1 && ok2 {
			labels[n] = v
		}
	}
	return labels
}
