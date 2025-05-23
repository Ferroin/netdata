// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NETDATA_EVENT_LOOP_H
#define NETDATA_EVENT_LOOP_H

enum event_loop_job {
    UV_EVENT_JOB_NONE = 0,

    // generic
    UV_EVENT_WORKER_INIT,

    // query related
    UV_EVENT_DBENGINE_QUERY,
    UV_EVENT_DBENGINE_EXTENT_CACHE_LOOKUP,
    UV_EVENT_DBENGINE_EXTENT_MMAP,
    UV_EVENT_DBENGINE_EXTENT_DECOMPRESSION,
    UV_EVENT_DBENGINE_EXTENT_PAGE_LOOKUP,
    UV_EVENT_DBENGINE_EXTENT_PAGE_POPULATION,
    UV_EVENT_DBENGINE_EXTENT_PAGE_ALLOCATION,

    // flushing related
    UV_EVENT_DBENGINE_FLUSH_MAIN_CACHE,
    UV_EVENT_DBENGINE_EXTENT_WRITE,
    UV_EVENT_DBENGINE_FLUSHED_TO_OPEN,

    // datafile full
    UV_EVENT_DBENGINE_JOURNAL_INDEX,

    // db rotation related
    UV_EVENT_DBENGINE_DATAFILE_DELETE_WAIT,
    UV_EVENT_DBENGINE_DATAFILE_DELETE,
    UV_EVENT_DBENGINE_FIND_ROTATED_METRICS, // find the metrics that are rotated
    UV_EVENT_DBENGINE_FIND_REMAINING_RETENTION, // find their remaining retention
    UV_EVENT_DBENGINE_POPULATE_MRG, // update mrg

    // other dbengine events
    UV_EVENT_DBENGINE_EVICT_MAIN_CACHE,
    UV_EVENT_DBENGINE_EVICT_OPEN_CACHE,
    UV_EVENT_DBENGINE_EVICT_EXTENT_CACHE,
    UV_EVENT_DBENGINE_BUFFERS_CLEANUP,
    UV_EVENT_DBENGINE_FLUSH_DIRTY,
    UV_EVENT_DBENGINE_QUIESCE,
    UV_EVENT_DBENGINE_SHUTDOWN,

    // metadata
    UV_EVENT_HOST_CONTEXT_LOAD,
    UV_EVENT_METADATA_STORE,
    UV_EVENT_METADATA_CLEANUP,
    UV_EVENT_METADATA_ML_LOAD,
    UV_EVENT_CTX_CLEANUP_SCHEDULE,
    UV_EVENT_CTX_CLEANUP,
    UV_EVENT_STORE_HOST,
    UV_EVENT_STORE_CHART,
    UV_EVENT_STORE_DIMENSION,
    UV_EVENT_STORE_ALERT_TRANSITIONS,
    UV_EVENT_STORE_SQL_STATEMENTS,
    UV_EVENT_HEALTH_LOG_CLEANUP,
    UV_EVENT_CHART_LABEL_CLEANUP,
    UV_EVENT_UUID_DELETION,
    UV_EVENT_DIMENSION_CLEANUP,
    UV_EVENT_CHART_CLEANUP,

    // aclk_sync
    UV_EVENT_ACLK_NODE_INFO,
    UV_EVENT_ACLK_ALERT_PUSH,
    UV_EVENT_ACLK_QUERY_EXECUTE,

    //
    UV_EVENT_CTX_STOP_STREAMING,
    UV_EVENT_CTX_CHECKPOINT,
    UV_EVENT_ALARM_PROVIDE_CFG,
    UV_EVENT_ALARM_SNAPSHOT,
    UV_EVENT_REGISTER_NODE,
    UV_EVENT_UPDATE_NODE_COLLECTORS,
    UV_EVENT_UPDATE_NODE_INFO,
    UV_EVENT_CTX_SEND_SNAPSHOT,
    UV_EVENT_CTX_SEND_SNAPSHOT_UPD,
    UV_EVENT_NODE_STATE_UPDATE,
    UV_EVENT_SEND_NODE_INSTANCES,
    UV_EVENT_ALERT_START_STREAMING,
    UV_EVENT_ALERT_CHECKPOINT,
    UV_EVENT_CREATE_NODE_INSTANCE,
    UV_EVENT_UNREGISTER_NODE,

    // netdatacli
    UV_EVENT_SCHEDULE_CMD,
};

void register_libuv_worker_jobs();
void libuv_close_callback(uv_handle_t *handle, void *data __maybe_unused);

#endif //NETDATA_EVENT_LOOP_H
