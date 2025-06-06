// SPDX-License-Identifier: GPL-3.0-or-later

#include "api_v2_contexts.h"
#include "libnetdata/json/json-keys.h"

struct alert_counts {
    size_t critical;
    size_t warning;
    size_t clear;
    size_t error;
};

struct alert_v2_entry {
    RRDCALC *tmp;

    STRING *name;
    STRING *summary;
    RRDLABELS *recipient;
    RRDLABELS *classification;
    RRDLABELS *context;
    RRDLABELS *component;
    RRDLABELS *type;

    size_t ati;

    struct alert_counts counts;

    size_t instances;
    DICTIONARY *nodes;
    DICTIONARY *configs;
};

struct alert_by_x_entry {
    struct {
        struct alert_counts counts;
        size_t silent;
        size_t total;
    } running;

    struct {
        size_t available;
    } prototypes;
};

bool rrdcontext_matches_alert(struct rrdcontext_to_json_v2_data *ctl, RRDCONTEXT *rc) {
    size_t matches = 0;
    RRDINSTANCE *ri;
    dfe_start_read(rc->rrdinstances, ri) {
        if(ri->rrdset) {
            RRDSET *st = ri->rrdset;
            rw_spinlock_read_lock(&st->alerts.spinlock);
            for (RRDCALC *rcl = st->alerts.base; rcl; rcl = rcl->next) {
                if(ctl->alerts.alert_name_pattern && !simple_pattern_matches_string(ctl->alerts.alert_name_pattern, rcl->config.name))
                    continue;

                if(ctl->alerts.alarm_id_filter && ctl->alerts.alarm_id_filter != (time_t)rcl->id)
                    continue;

                size_t m = ctl->request->alerts.status & CONTEXTS_ALERT_STATUSES ? 0 : 1;

                if (!m) {
                    if ((ctl->request->alerts.status & CONTEXT_ALERT_UNINITIALIZED) &&
                        rcl->status == RRDCALC_STATUS_UNINITIALIZED)
                        m++;

                    if ((ctl->request->alerts.status & CONTEXT_ALERT_UNDEFINED) &&
                        rcl->status == RRDCALC_STATUS_UNDEFINED)
                        m++;

                    if ((ctl->request->alerts.status & CONTEXT_ALERT_CLEAR) &&
                        rcl->status == RRDCALC_STATUS_CLEAR)
                        m++;

                    if ((ctl->request->alerts.status & CONTEXT_ALERT_RAISED) &&
                        rcl->status >= RRDCALC_STATUS_RAISED)
                        m++;

                    if ((ctl->request->alerts.status & CONTEXT_ALERT_WARNING) &&
                        rcl->status == RRDCALC_STATUS_WARNING)
                        m++;

                    if ((ctl->request->alerts.status & CONTEXT_ALERT_CRITICAL) &&
                        rcl->status == RRDCALC_STATUS_CRITICAL)
                        m++;

                    if(!m)
                        continue;
                }

                struct alert_v2_entry t = {
                    .tmp = rcl,
                };
                struct alert_v2_entry *a2e =
                    dictionary_set(ctl->alerts.summary, string2str(rcl->config.name),
                                   &t, sizeof(struct alert_v2_entry));
                size_t ati = a2e->ati;
                matches++;

                dictionary_set_advanced(ctl->alerts.by_type,
                                        string2str(rcl->config.type),
                                        (ssize_t)string_strlen(rcl->config.type),
                                        NULL,
                                        sizeof(struct alert_by_x_entry),
                                        rcl);

                dictionary_set_advanced(ctl->alerts.by_component,
                                        string2str(rcl->config.component),
                                        (ssize_t)string_strlen(rcl->config.component),
                                        NULL,
                                        sizeof(struct alert_by_x_entry),
                                        rcl);

                dictionary_set_advanced(ctl->alerts.by_classification,
                                        string2str(rcl->config.classification),
                                        (ssize_t)string_strlen(rcl->config.classification),
                                        NULL,
                                        sizeof(struct alert_by_x_entry),
                                        rcl);

                dictionary_set_advanced(ctl->alerts.by_recipient,
                                        string2str(rcl->config.recipient),
                                        (ssize_t)string_strlen(rcl->config.recipient),
                                        NULL,
                                        sizeof(struct alert_by_x_entry),
                                        rcl);

                char module[128];
                rrdlabels_get_value_strcpyz(st->rrdlabels, module, sizeof(module), "_collect_module");
                if(!*module)
                    strncpyz(module, "[unset]", sizeof(module) - 1);

                dictionary_set_advanced(ctl->alerts.by_module,
                                        module,
                                        -1,
                                        NULL,
                                        sizeof(struct alert_by_x_entry),
                                        rcl);

                if (ctl->options & (CONTEXTS_OPTION_INSTANCES | CONTEXTS_OPTION_VALUES)) {
                    char key[20 + 1];
                    snprintfz(key, sizeof(key) - 1, "%p", rcl);

                    struct sql_alert_instance_v2_entry z = {
                        .ati = ati,
                        .tmp = rcl,
                    };
                    dictionary_set(ctl->alerts.alert_instances, key, &z, sizeof(z));
                }
            }
            rw_spinlock_read_unlock(&st->alerts.spinlock);
        }
    }
    dfe_done(ri);

    return matches != 0;
}

static void alert_counts_add(struct alert_counts *t, RRDCALC *rc) {
    switch(rc->status) {
        case RRDCALC_STATUS_CRITICAL:
            t->critical++;
            break;

        case RRDCALC_STATUS_WARNING:
            t->warning++;
            break;

        case RRDCALC_STATUS_CLEAR:
            t->clear++;
            break;

        case RRDCALC_STATUS_REMOVED:
        case RRDCALC_STATUS_UNINITIALIZED:
            break;

        case RRDCALC_STATUS_UNDEFINED:
        default:
            if(!netdata_double_isnumber(rc->value))
                t->error++;

            break;
    }
}

static void alerts_v2_add(struct alert_v2_entry *t, RRDCALC *rc) {
    t->instances++;

    alert_counts_add(&t->counts, rc);

    dictionary_set(t->nodes, rc->rrdset->rrdhost->machine_guid, NULL, 0);

    char key[UUID_STR_LEN + 1];
    uuid_unparse_lower(rc->config.hash_id, key);
    dictionary_set(t->configs, key, NULL, 0);
}

static STRING *silent_string = NULL;
void alerts_by_x_cleanup(void) {
    string_freez(silent_string);
    silent_string = NULL;
}

static void alerts_by_x_insert_callback(const DICTIONARY_ITEM *item __maybe_unused, void *value, void *data) {
    if(unlikely(!silent_string))
        silent_string = string_strdupz("silent");

    struct alert_by_x_entry *b = value;
    RRDCALC *rc = data;
    if(!rc) {
        // prototype
        b->prototypes.available++;
    }
    else {
        alert_counts_add(&b->running.counts, rc);

        b->running.total++;

        if (rc->config.recipient == silent_string)
            b->running.silent++;
    }
}

static bool alerts_by_x_conflict_callback(const DICTIONARY_ITEM *item __maybe_unused, void *old_value, void *new_value __maybe_unused, void *data __maybe_unused) {
    alerts_by_x_insert_callback(item, old_value, data);
    return false;
}

static void alerts_v2_insert_callback(const DICTIONARY_ITEM *item __maybe_unused, void *value, void *data) {
    struct rrdcontext_to_json_v2_data *ctl = data;
    struct alert_v2_entry *t = value;
    RRDCALC *rc = t->tmp;
    t->name = rc->config.name;
    t->summary = rc->config.summary; // the original summary
    t->context = rrdlabels_create();
    t->recipient = rrdlabels_create();
    t->classification = rrdlabels_create();
    t->component = rrdlabels_create();
    t->type = rrdlabels_create();
    if (string_strlen(rc->rrdset->context))
        rrdlabels_add(t->context, string2str(rc->rrdset->context), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.recipient))
        rrdlabels_add(t->recipient, string2str(rc->config.recipient), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.classification))
        rrdlabels_add(t->classification, string2str(rc->config.classification), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.component))
        rrdlabels_add(t->component, string2str(rc->config.component), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.type))
        rrdlabels_add(t->type, string2str(rc->config.type), "yes", RRDLABEL_SRC_AUTO);
    t->ati = ctl->alerts.ati++;

    t->nodes = dictionary_create(DICT_OPTION_SINGLE_THREADED|DICT_OPTION_VALUE_LINK_DONT_CLONE|DICT_OPTION_NAME_LINK_DONT_CLONE);
    t->configs = dictionary_create(DICT_OPTION_SINGLE_THREADED|DICT_OPTION_VALUE_LINK_DONT_CLONE|DICT_OPTION_NAME_LINK_DONT_CLONE);

    alerts_v2_add(t, rc);
}

static bool alerts_v2_conflict_callback(const DICTIONARY_ITEM *item __maybe_unused, void *old_value, void *new_value, void *data __maybe_unused) {
    struct alert_v2_entry *t = old_value, *n = new_value;
    RRDCALC *rc = n->tmp;
    if (string_strlen(rc->rrdset->context))
        rrdlabels_add(t->context, string2str(rc->rrdset->context), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.recipient))
        rrdlabels_add(t->recipient, string2str(rc->config.recipient), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.classification))
        rrdlabels_add(t->classification, string2str(rc->config.classification), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.component))
        rrdlabels_add(t->component, string2str(rc->config.component), "yes", RRDLABEL_SRC_AUTO);
    if (string_strlen(rc->config.type))
        rrdlabels_add(t->type, string2str(rc->config.type), "yes", RRDLABEL_SRC_AUTO);
    alerts_v2_add(t, rc);
    return true;
}

static void alerts_v2_delete_callback(const DICTIONARY_ITEM *item __maybe_unused, void *value, void *data __maybe_unused) {
    struct alert_v2_entry *t = value;

    rrdlabels_destroy(t->context);
    rrdlabels_destroy(t->recipient);
    rrdlabels_destroy(t->classification);
    rrdlabels_destroy(t->component);
    rrdlabels_destroy(t->type);

    dictionary_destroy(t->nodes);
    dictionary_destroy(t->configs);
}

struct alert_instances_callback_data {
    BUFFER *wb;
    struct rrdcontext_to_json_v2_data *ctl;
    bool debug;
};

static int contexts_v2_alert_instance_to_json_callback(const DICTIONARY_ITEM *item __maybe_unused, void *value, void *data) {
    struct sql_alert_instance_v2_entry *t = value;
    struct alert_instances_callback_data *d = data;
    struct rrdcontext_to_json_v2_data *ctl = d->ctl; (void)ctl;
    bool debug = d->debug; (void)debug;
    BUFFER *wb = d->wb;

    buffer_json_add_array_item_object(wb);
    {
        if(ctl->request->options & CONTEXTS_OPTION_SUMMARY)
            buffer_json_member_add_uint64(wb, JSKEY(alerts_index_id), t->ati);

        if(!(ctl->request->options & CONTEXTS_OPTION_MCP))
            buffer_json_member_add_uint64(wb, JSKEY(node_index), t->ni);

        if(ctl->request->options & CONTEXTS_OPTION_INSTANCES)
            buffer_json_member_add_uint64(wb, JSKEY(alert_global_id), t->global_id);

        buffer_json_member_add_string(wb, JSKEY(alert_name), string2str(t->name));

        if(ctl->request->options & CONTEXTS_OPTION_MCP)
            buffer_json_member_add_string(wb, JSKEY(hostname), string2str(t->host->hostname));

        if(ctl->request->options & CONTEXTS_OPTION_INSTANCES)
            buffer_json_member_add_string(wb, JSKEY(context), string2str(t->context));

        if(!(ctl->request->options & CONTEXTS_OPTION_MCP))
            buffer_json_member_add_string(wb, JSKEY(instance_id), string2str(t->chart_id));

        buffer_json_member_add_string(wb, JSKEY(instance_name), string2str(t->chart_name));

        if(ctl->request->options & CONTEXTS_OPTION_INSTANCES)
            buffer_json_member_add_string(wb, JSKEY(status), rrdcalc_status2string(t->status));

        if(ctl->request->options & CONTEXTS_OPTION_INSTANCES) {
            buffer_json_member_add_string(wb, JSKEY(family), string2str(t->family));
            buffer_json_member_add_string(wb, "info", string2str(t->info));
            buffer_json_member_add_string(wb, JSKEY(summary), string2str(t->summary));
            buffer_json_member_add_string(wb, "units", string2str(t->units));
            buffer_json_member_add_uuid(wb, JSKEY(last_transition_id), t->last_transition_id);
            buffer_json_member_add_double(wb,  JSKEY(last_transition_value), t->last_status_change_value);
            buffer_json_member_add_time_t_formatted(wb, JSKEY(last_transition_timestamp), t->last_status_change, ctl->options & CONTEXTS_OPTION_RFC3339);
            buffer_json_member_add_uuid(wb, JSKEY(config_hash_id), t->config_hash_id);
            buffer_json_member_add_string(wb, JSKEY(source), string2str(t->source));

            buffer_json_member_add_string(wb, JSKEY(recipients), string2str(t->recipient));
            buffer_json_member_add_string(wb, JSKEY(type), string2str(t->type));
            buffer_json_member_add_string(wb, JSKEY(component), string2str(t->component));
            buffer_json_member_add_string(wb, JSKEY(classification), string2str(t->classification));

            // rrdcalc_flags_to_json_array  (wb, "flags", t->flags);
        }

        if(ctl->request->options & CONTEXTS_OPTION_VALUES) {
            // Netdata Cloud fetched these by querying the agents
            buffer_json_member_add_double(wb, JSKEY(last_updated_value), t->value);
            buffer_json_member_add_time_t_formatted(wb, JSKEY(last_updated_timestamp), t->last_updated, ctl->options & CONTEXTS_OPTION_RFC3339);
        }
    }
    buffer_json_object_close(wb); // alert instance

    return 1;
}

static void contexts_v2_alerts_by_x_update_prototypes(void *data, STRING *type, STRING *component, STRING *classification, STRING *recipient) {
    struct rrdcontext_to_json_v2_data *ctl = data;

    dictionary_set_advanced(ctl->alerts.by_type, string2str(type), (ssize_t)string_strlen(type), NULL, sizeof(struct alert_by_x_entry), NULL);
    dictionary_set_advanced(ctl->alerts.by_component, string2str(component), (ssize_t)string_strlen(component), NULL, sizeof(struct alert_by_x_entry), NULL);
    dictionary_set_advanced(ctl->alerts.by_classification, string2str(classification), (ssize_t)string_strlen(classification), NULL, sizeof(struct alert_by_x_entry), NULL);
    dictionary_set_advanced(ctl->alerts.by_recipient, string2str(recipient), (ssize_t)string_strlen(recipient), NULL, sizeof(struct alert_by_x_entry), NULL);
}

static void contexts_v2_alerts_by_x_to_json(BUFFER *wb, DICTIONARY *dict, const char *key) {
    buffer_json_member_add_array(wb, key);
    {
        struct alert_by_x_entry *b;
        dfe_start_read(dict, b) {
            buffer_json_add_array_item_object(wb);
            {
                buffer_json_member_add_string(wb, "name", b_dfe.name);
                buffer_json_member_add_uint64(wb, JSKEY(critical), b->running.counts.critical);
                buffer_json_member_add_uint64(wb, JSKEY(warning), b->running.counts.warning);
                buffer_json_member_add_uint64(wb, JSKEY(clear), b->running.counts.clear);
                buffer_json_member_add_uint64(wb, JSKEY(error), b->running.counts.error);
                buffer_json_member_add_uint64(wb, "running", b->running.total);

                buffer_json_member_add_uint64(wb, "running_silent", b->running.silent);

                if(b->prototypes.available)
                    buffer_json_member_add_uint64(wb, "available", b->prototypes.available);
            }
            buffer_json_object_close(wb);
        }
        dfe_done(b);
    }
    buffer_json_array_close(wb);
}

static void contexts_v2_alert_instances_to_json(BUFFER *wb, const char *key, struct rrdcontext_to_json_v2_data *ctl, bool debug) {
    buffer_json_member_add_array(wb, key);
    {
        struct alert_instances_callback_data data = {
            .wb = wb,
            .ctl = ctl,
            .debug = debug,
        };
        dictionary_walkthrough_rw(ctl->alerts.alert_instances, DICTIONARY_LOCK_READ,
                                  contexts_v2_alert_instance_to_json_callback, &data);
    }
    buffer_json_array_close(wb); // alerts_instances
}

void contexts_v2_alerts_to_json_mcp(BUFFER *wb, struct rrdcontext_to_json_v2_data *ctl, bool debug __maybe_unused) {
    size_t cardinality_limit = ctl->request->cardinality_limit;
    
    if(ctl->request->options & CONTEXTS_OPTION_SUMMARY) {
        size_t alerts_count = 0;
        size_t total_alerts = dictionary_entries(ctl->alerts.summary);
        
        // Add header array
        buffer_json_member_add_array(wb, "all_alerts_header");
        {
            buffer_json_add_array_item_string(wb, "Alert Name");
            buffer_json_add_array_item_string(wb, "Alert Summary");

            buffer_json_add_array_item_string(wb, "Metrics Contexts");
            buffer_json_add_array_item_string(wb, "Alert Classifications");
            buffer_json_add_array_item_string(wb, "Alert Components");
            buffer_json_add_array_item_string(wb, "Alert Types");
            buffer_json_add_array_item_string(wb, "Notification Recipients");

            buffer_json_add_array_item_string(wb, "# of Critical Instances");
            buffer_json_add_array_item_string(wb, "# of Warning Instances");
            buffer_json_add_array_item_string(wb, "# of Clear Instances");
            buffer_json_add_array_item_string(wb, "# of Error Instances");
            buffer_json_add_array_item_string(wb, "# of Instances Watched");
            buffer_json_add_array_item_string(wb, "# of Nodes Watched");
            buffer_json_add_array_item_string(wb, "# of Alert Configurations");
        }
        buffer_json_array_close(wb); // header

        // Add data array
        buffer_json_member_add_array(wb, "all_alerts");
        struct alert_v2_entry *t;
        dfe_start_read(ctl->alerts.summary, t)
        {
            // Apply cardinality limit for summary
            if(cardinality_limit && alerts_count >= cardinality_limit)
                break;
                
            // Start row array
            buffer_json_add_array_item_array(wb);
            {
                // Add values in same order as header
                buffer_json_add_array_item_string(wb, string2str(t->name));
                buffer_json_add_array_item_string(wb, string2str(t->summary));

                rrdlabels_key_to_buffer_array_or_string_or_null(t->context, wb);
                rrdlabels_key_to_buffer_array_or_string_or_null(t->classification, wb);
                rrdlabels_key_to_buffer_array_or_string_or_null(t->component, wb);
                rrdlabels_key_to_buffer_array_or_string_or_null(t->type, wb);
                rrdlabels_key_to_buffer_array_or_string_or_null(t->recipient, wb);

                buffer_json_add_array_item_uint64(wb, t->counts.critical);
                buffer_json_add_array_item_uint64(wb, t->counts.warning);
                buffer_json_add_array_item_uint64(wb, t->counts.clear);
                buffer_json_add_array_item_uint64(wb, t->counts.error);
                buffer_json_add_array_item_uint64(wb, t->instances);
                buffer_json_add_array_item_uint64(wb, dictionary_entries(t->nodes));
                buffer_json_add_array_item_uint64(wb, dictionary_entries(t->configs));
            }
            buffer_json_array_close(wb);
            alerts_count++;
        }
        dfe_done(t);
        buffer_json_array_close(wb); // all_alerts
        
        // Add info about truncation if needed
        if(cardinality_limit && total_alerts > cardinality_limit) {
            buffer_json_member_add_object(wb, "__all_alerts_info__");
            buffer_json_member_add_string(wb, "status", "truncated");
            buffer_json_member_add_uint64(wb, "total_alerts", total_alerts);
            buffer_json_member_add_uint64(wb, "shown_alerts", alerts_count);
            buffer_json_member_add_uint64(wb, "cardinality_limit", cardinality_limit);
            buffer_json_object_close(wb);
        }
    }

    if(ctl->request->options & (CONTEXTS_OPTION_INSTANCES | CONTEXTS_OPTION_VALUES)) {
        size_t instances_count = 0;
        size_t total_instances = dictionary_entries(ctl->alerts.alert_instances);
        
        // Add header array for alert instances
        buffer_json_member_add_array(wb, "alert_instances_header");
        {
            buffer_json_add_array_item_string(wb, "Alert Name");
            buffer_json_add_array_item_string(wb, "Hostname");
            
            if(ctl->request->options & CONTEXTS_OPTION_INSTANCES)
                buffer_json_add_array_item_string(wb, "Context");
            
            buffer_json_add_array_item_string(wb, "Instance Name");
            
            if(ctl->request->options & CONTEXTS_OPTION_INSTANCES) {
                buffer_json_add_array_item_string(wb, "Status");
                buffer_json_add_array_item_string(wb, "Family");
                buffer_json_add_array_item_string(wb, "Info");
                buffer_json_add_array_item_string(wb, "Summary");
                buffer_json_add_array_item_string(wb, "Units");
                buffer_json_add_array_item_string(wb, "Last Transition ID");
                buffer_json_add_array_item_string(wb, "Last Transition Value");
                buffer_json_add_array_item_string(wb, "Last Transition Timestamp");
                buffer_json_add_array_item_string(wb, "Configuration Hash");
                buffer_json_add_array_item_string(wb, "Source");
                buffer_json_add_array_item_string(wb, "Recipients");
                buffer_json_add_array_item_string(wb, "Type");
                buffer_json_add_array_item_string(wb, "Component");
                buffer_json_add_array_item_string(wb, "Classification");
            }
            
            if(ctl->request->options & CONTEXTS_OPTION_VALUES) {
                buffer_json_add_array_item_string(wb, "Last Updated Value");
                buffer_json_add_array_item_string(wb, "Last Updated Timestamp");
            }
        }
        buffer_json_array_close(wb); // alert_instances_header
        
        // Add data array for alert instances
        buffer_json_member_add_array(wb, "alert_instances");
        {
            struct sql_alert_instance_v2_entry *t;
            dfe_start_read(ctl->alerts.alert_instances, t)
            {
                // Apply cardinality limit for instances
                if(cardinality_limit && instances_count >= cardinality_limit)
                    break;
                    
                // Start row array
                buffer_json_add_array_item_array(wb);
                {
                    buffer_json_add_array_item_string(wb, string2str(t->name));
                    buffer_json_add_array_item_string(wb, string2str(t->host->hostname));
                    
                    if(ctl->request->options & CONTEXTS_OPTION_INSTANCES)
                        buffer_json_add_array_item_string(wb, string2str(t->context));
                    
                    buffer_json_add_array_item_string(wb, string2str(t->chart_name));
                    
                    if(ctl->request->options & CONTEXTS_OPTION_INSTANCES) {
                        buffer_json_add_array_item_string(wb, rrdcalc_status2string(t->status));
                        buffer_json_add_array_item_string(wb, string2str(t->family));
                        buffer_json_add_array_item_string(wb, string2str(t->info));
                        buffer_json_add_array_item_string(wb, string2str(t->summary));
                        buffer_json_add_array_item_string(wb, string2str(t->units));
                        
                        // Add UUID as string
                        char uuid_str[UUID_STR_LEN];
                        uuid_unparse_lower(t->last_transition_id, uuid_str);
                        buffer_json_add_array_item_string(wb, uuid_str);
                        
                        buffer_json_add_array_item_double(wb, t->last_status_change_value);
                        buffer_json_add_array_item_time_t_formatted(wb, t->last_status_change, ctl->options & CONTEXTS_OPTION_RFC3339);
                        
                        // Add config hash UUID as string
                        uuid_unparse_lower(t->config_hash_id, uuid_str);
                        buffer_json_add_array_item_string(wb, uuid_str);
                        
                        buffer_json_add_array_item_string(wb, string2str(t->source));
                        buffer_json_add_array_item_string(wb, string2str(t->recipient));
                        buffer_json_add_array_item_string(wb, string2str(t->type));
                        buffer_json_add_array_item_string(wb, string2str(t->component));
                        buffer_json_add_array_item_string(wb, string2str(t->classification));
                    }
                    
                    if(ctl->request->options & CONTEXTS_OPTION_VALUES) {
                        buffer_json_add_array_item_double(wb, t->value);
                        buffer_json_add_array_item_time_t_formatted(wb, t->last_updated, ctl->options & CONTEXTS_OPTION_RFC3339);
                    }
                }
                buffer_json_array_close(wb); // row
                instances_count++;
            }
            dfe_done(t);
        }
        buffer_json_array_close(wb); // alert_instances
        
        // Add info about truncation if needed
        if(cardinality_limit && total_instances > cardinality_limit) {
            buffer_json_member_add_object(wb, "__alert_instances_info__");
            buffer_json_member_add_string(wb, "status", "truncated");
            buffer_json_member_add_uint64(wb, "total_instances", total_instances);
            buffer_json_member_add_uint64(wb, "shown_instances", instances_count);
            buffer_json_member_add_uint64(wb, "cardinality_limit", cardinality_limit);
            buffer_json_object_close(wb);
        }
    }

}

void contexts_v2_alerts_to_json(BUFFER *wb, struct rrdcontext_to_json_v2_data *ctl, bool debug) {
    if(ctl->request->options & CONTEXTS_OPTION_MCP) {
        contexts_v2_alerts_to_json_mcp(wb, ctl, debug);
        return;
    }

    if(ctl->request->options & CONTEXTS_OPTION_SUMMARY) {
        buffer_json_member_add_array(wb, "alerts");
        {
            struct alert_v2_entry *t;
            dfe_start_read(ctl->alerts.summary, t)
            {
                buffer_json_add_array_item_object(wb);
                {
                    buffer_json_member_add_uint64(wb, JSKEY(alerts_index_id), t->ati);

                    buffer_json_member_add_array(wb, JSKEY(node_index));
                    void *host_guid;
                    dfe_start_read(t->nodes, host_guid) {
                        struct contexts_v2_node *cn = dictionary_get(ctl->nodes.dict, host_guid_dfe.name);
                        buffer_json_add_array_item_int64(wb, (int64_t) cn->ni);
                    }
                    dfe_done(host_guid);
                    buffer_json_array_close(wb);

                    buffer_json_member_add_string(wb, JSKEY(alert_name), string2str(t->name));
                    buffer_json_member_add_string(wb, JSKEY(summary), string2str(t->summary));

                    buffer_json_member_add_uint64(wb, JSKEY(critical), t->counts.critical);
                    buffer_json_member_add_uint64(wb, JSKEY(warning), t->counts.warning);
                    buffer_json_member_add_uint64(wb, JSKEY(clear), t->counts.clear);
                    buffer_json_member_add_uint64(wb, JSKEY(error), t->counts.error);

                    buffer_json_member_add_uint64(wb, JSKEY(instances_count), t->instances);
                    buffer_json_member_add_uint64(wb, JSKEY(nodes_count), dictionary_entries(t->nodes));
                    buffer_json_member_add_uint64(wb, JSKEY(configurations_count), dictionary_entries(t->configs));

                    buffer_json_member_add_array(wb, JSKEY(contexts));
                    rrdlabels_key_to_buffer_array_item(t->context, wb);
                    buffer_json_array_close(wb); // contexts

                    buffer_json_member_add_array(wb, JSKEY(classifications));
                    rrdlabels_key_to_buffer_array_item(t->classification, wb);
                    buffer_json_array_close(wb); // classifications


                    buffer_json_member_add_array(wb, JSKEY(components));
                    rrdlabels_key_to_buffer_array_item(t->component, wb);
                    buffer_json_array_close(wb); // components

                    buffer_json_member_add_array(wb, JSKEY(types));
                    rrdlabels_key_to_buffer_array_item(t->type, wb);
                    buffer_json_array_close(wb); // types

                    buffer_json_member_add_array(wb, JSKEY(recipients));
                    rrdlabels_key_to_buffer_array_item(t->recipient, wb);
                    buffer_json_array_close(wb); // recipients
                }
                buffer_json_object_close(wb); // alert name
            }
            dfe_done(t);
        }
        buffer_json_array_close(wb); // alerts

        health_prototype_metadata_foreach(ctl, contexts_v2_alerts_by_x_update_prototypes);
        contexts_v2_alerts_by_x_to_json(wb, ctl->alerts.by_type, "alerts_by_type");
        contexts_v2_alerts_by_x_to_json(wb, ctl->alerts.by_component, "alerts_by_component");
        contexts_v2_alerts_by_x_to_json(wb, ctl->alerts.by_classification, "alerts_by_classification");
        contexts_v2_alerts_by_x_to_json(wb, ctl->alerts.by_recipient, "alerts_by_recipient");
        contexts_v2_alerts_by_x_to_json(wb, ctl->alerts.by_module, "alerts_by_module");
    }

    if(ctl->request->options & (CONTEXTS_OPTION_INSTANCES | CONTEXTS_OPTION_VALUES)) {
        contexts_v2_alert_instances_to_json(wb, "alert_instances", ctl, debug);
    }
}

static void alert_instances_v2_insert_callback(const DICTIONARY_ITEM *item __maybe_unused, void *value, void *data) {
    struct rrdcontext_to_json_v2_data *ctl = data;
    struct sql_alert_instance_v2_entry *t = value;
    RRDCALC *rc = t->tmp;

    t->context = rc->rrdset->context;
    t->chart_id = rc->rrdset->id;
    t->chart_name = rc->rrdset->name;
    t->family = rc->rrdset->family;
    t->units = rc->config.units;
    t->classification = rc->config.classification;
    t->type = rc->config.type;
    t->recipient = rc->config.recipient;
    t->component = rc->config.component;
    t->name = rc->config.name;
    t->source = rc->config.source;
    t->status = rc->status;
    t->flags = rc->run_flags;
    t->info = rc->config.info;
    t->summary = rc->summary;
    t->value = rc->value;
    t->last_updated = rc->last_updated;
    t->last_status_change = rc->last_status_change;
    t->last_status_change_value = rc->last_status_change_value;
    t->host = rc->rrdset->rrdhost;
    t->alarm_id = rc->id;
    t->ni = ctl->nodes.ni;

    uuid_copy(t->config_hash_id, rc->config.hash_id);
    health_alarm_log_get_global_id_and_transition_id_for_rrdcalc(rc, &t->global_id, &t->last_transition_id);
}

static bool alert_instances_v2_conflict_callback(const DICTIONARY_ITEM *item __maybe_unused, void *old_value __maybe_unused, void *new_value __maybe_unused, void *data __maybe_unused) {
    internal_fatal(true, "This should never happen!");
    return true;
}

static void alert_instances_delete_callback(const DICTIONARY_ITEM *item __maybe_unused, void *value __maybe_unused, void *data __maybe_unused) {
    ;
}

static void rrdcontext_v2_set_transition_filter(const char *machine_guid, const char *context, time_t alarm_id, void *data) {
    struct rrdcontext_to_json_v2_data *ctl = data;

    if(machine_guid && *machine_guid) {
        if(ctl->nodes.scope_pattern)
            simple_pattern_free(ctl->nodes.scope_pattern);

        if(ctl->nodes.pattern)
            simple_pattern_free(ctl->nodes.pattern);

        ctl->nodes.scope_pattern = string_to_simple_pattern(machine_guid);
        ctl->nodes.pattern = NULL;
    }

    if(context && *context) {
        if(ctl->contexts.scope_pattern)
            simple_pattern_free(ctl->contexts.scope_pattern);

        if(ctl->contexts.pattern)
            simple_pattern_free(ctl->contexts.pattern);

        ctl->contexts.scope_pattern = string_to_simple_pattern(context);
        ctl->contexts.pattern = NULL;
    }

    ctl->alerts.alarm_id_filter = alarm_id;
}

bool rrdcontexts_v2_init_alert_dictionaries(struct rrdcontext_to_json_v2_data *ctl, struct api_v2_contexts_request *req) {
    if(req->alerts.transition) {
        ctl->options |= CONTEXTS_OPTION_INSTANCES | CONTEXTS_OPTION_VALUES;
        if(!sql_find_alert_transition(req->alerts.transition, rrdcontext_v2_set_transition_filter, ctl))
            return false;
    }

    ctl->alerts.summary = dictionary_create_advanced(
        DICT_OPTION_SINGLE_THREADED | DICT_OPTION_DONT_OVERWRITE_VALUE | DICT_OPTION_FIXED_SIZE,
        NULL,
        sizeof(struct alert_v2_entry));

    dictionary_register_insert_callback(ctl->alerts.summary, alerts_v2_insert_callback, ctl);
    dictionary_register_conflict_callback(ctl->alerts.summary, alerts_v2_conflict_callback, ctl);
    dictionary_register_delete_callback(ctl->alerts.summary, alerts_v2_delete_callback, ctl);

    ctl->alerts.by_type = dictionary_create_advanced(
        DICT_OPTION_SINGLE_THREADED | DICT_OPTION_DONT_OVERWRITE_VALUE | DICT_OPTION_FIXED_SIZE,
        NULL,
        sizeof(struct alert_by_x_entry));

    dictionary_register_insert_callback(ctl->alerts.by_type, alerts_by_x_insert_callback, NULL);
    dictionary_register_conflict_callback(ctl->alerts.by_type, alerts_by_x_conflict_callback, NULL);

    ctl->alerts.by_component = dictionary_create_advanced(
        DICT_OPTION_SINGLE_THREADED | DICT_OPTION_DONT_OVERWRITE_VALUE | DICT_OPTION_FIXED_SIZE,
        NULL,
        sizeof(struct alert_by_x_entry));

    dictionary_register_insert_callback(ctl->alerts.by_component, alerts_by_x_insert_callback, NULL);
    dictionary_register_conflict_callback(ctl->alerts.by_component, alerts_by_x_conflict_callback, NULL);

    ctl->alerts.by_classification = dictionary_create_advanced(
        DICT_OPTION_SINGLE_THREADED | DICT_OPTION_DONT_OVERWRITE_VALUE | DICT_OPTION_FIXED_SIZE,
        NULL,
        sizeof(struct alert_by_x_entry));

    dictionary_register_insert_callback(ctl->alerts.by_classification, alerts_by_x_insert_callback, NULL);
    dictionary_register_conflict_callback(ctl->alerts.by_classification, alerts_by_x_conflict_callback, NULL);

    ctl->alerts.by_recipient = dictionary_create_advanced(
        DICT_OPTION_SINGLE_THREADED | DICT_OPTION_DONT_OVERWRITE_VALUE | DICT_OPTION_FIXED_SIZE,
        NULL,
        sizeof(struct alert_by_x_entry));

    dictionary_register_insert_callback(ctl->alerts.by_recipient, alerts_by_x_insert_callback, NULL);
    dictionary_register_conflict_callback(ctl->alerts.by_recipient, alerts_by_x_conflict_callback, NULL);

    ctl->alerts.by_module = dictionary_create_advanced(
        DICT_OPTION_SINGLE_THREADED | DICT_OPTION_DONT_OVERWRITE_VALUE | DICT_OPTION_FIXED_SIZE,
        NULL,
        sizeof(struct alert_by_x_entry));

    dictionary_register_insert_callback(ctl->alerts.by_module, alerts_by_x_insert_callback, NULL);
    dictionary_register_conflict_callback(ctl->alerts.by_module, alerts_by_x_conflict_callback, NULL);

    if(ctl->options & (CONTEXTS_OPTION_INSTANCES | CONTEXTS_OPTION_VALUES)) {
        ctl->alerts.alert_instances = dictionary_create_advanced(
            DICT_OPTION_SINGLE_THREADED | DICT_OPTION_DONT_OVERWRITE_VALUE | DICT_OPTION_FIXED_SIZE,
            NULL, sizeof(struct sql_alert_instance_v2_entry));

        dictionary_register_insert_callback(ctl->alerts.alert_instances, alert_instances_v2_insert_callback, ctl);
        dictionary_register_conflict_callback(ctl->alerts.alert_instances, alert_instances_v2_conflict_callback, ctl);
        dictionary_register_delete_callback(ctl->alerts.alert_instances, alert_instances_delete_callback, ctl);
    }

    return true;
}

void rrdcontexts_v2_alerts_cleanup(struct rrdcontext_to_json_v2_data *ctl) {
    dictionary_destroy(ctl->alerts.summary);
    dictionary_destroy(ctl->alerts.alert_instances);
    dictionary_destroy(ctl->alerts.by_type);
    dictionary_destroy(ctl->alerts.by_component);
    dictionary_destroy(ctl->alerts.by_classification);
    dictionary_destroy(ctl->alerts.by_recipient);
    dictionary_destroy(ctl->alerts.by_module);
}
