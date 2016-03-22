/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implement the functions related to loading an ETW log file and 
/// parsing the various types of events recognized by the profiler.
///////////////////////////////////////////////////////////////////////////80*/

/*//////////////////
//   Data Types   //
//////////////////*/

/*///////////////
//   Globals   //
///////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initialize an object lifetime record.
/// @param lifetime The record to initialize.
/// @param create_time The creation timestamp, in ticks. A value of 0 indicates that the object was created before the trace was started.
/// @param destroy_time The destruction timetsamp, in ticks. A value of 0 indicates that the object was destroyed after the trace ended.
public_function inline void
InitObjectLifetime
(
    WIN32_LIFETIME   &lifetime, 
    uint64_t       create_time=0, 
    uint64_t      destroy_time=0
)
{
    lifetime.CreateTime  = create_time;
    lifetime.DestroyTime = destroy_time;
}

/// @summary Update an object lifetime record with the time at which the object was created.
/// @param lifetime The record to update.
/// @param timestamp The object creation timestamp, in ticks.
public_function inline void
ObjectCreated
(
    WIN32_LIFETIME &lifetime, 
    LARGE_INTEGER  timestamp
)
{
    lifetime.CreateTime = uint64_t(timestamp.QuadPart);
}

/// @summary Update an object lifetime record with the time at which the object was destroyed.
/// @param lifetime The record to update.
/// @param timestamp The object destruction timestamp, in ticks.
public_function inline void
ObjectDestroyed
(
    WIN32_LIFETIME &lifetime, 
    LARGE_INTEGER  timestamp
)
{
    lifetime.DestroyTime = uint64_t(timestamp.QuadPart);
}

/// @summary Determine whether an object was alive at a given time.
/// @param lifetime The object lifetime record.
/// @param timestamp The timestamp, in ticks, indicating the point in time to query.
/// @return true if the object was alive at the specified point in time.
public_function inline bool
ObjectAliveAtTime
(
    WIN32_LIFETIME const &lifetime, 
    uint64_t       const timestamp
)
{   // check for timestamp >= CreateTime and <= DestroyTime.
    // a DestroyTime of 0 indicates that the object was still alive when capture stopped.
    return (timestamp >= lifetime.CreateTime && (lifetime.DestroyTime == 0 || lifetime.DestroyTime >= timestamp));
}

/// @summary Determine whether an object was alive at a given time.
/// @param lifetime The object lifetime record.
/// @param timestamp The timestamp, in ticks, indicating the point in time to query.
/// @return true if the object was alive at the specified point in time.
public_function inline bool
ObjectAliveAtTime
(
    WIN32_LIFETIME const &lifetime, 
    LARGE_INTEGER        timestamp
)
{
    return ObjectAliveAtTime(lifetime, uint64_t(timestamp.QuadPart));
}

/// @summary Retrieve a 32-bit unsigned integer property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The integer value.
internal_function uint32_t
ProfilerGetUInt32
(
    EVENT_RECORD           *ev, 
    TRACE_EVENT_INFO *info_buf, 
    size_t               index
)
{
    PROPERTY_DATA_DESCRIPTOR dd;
    uint32_t  value =  0;
    dd.PropertyName = (ULONGLONG)((uint8_t*) info_buf + info_buf->EventPropertyInfoArray[index].NameOffset);
    dd.ArrayIndex   =  ULONG_MAX;
    dd.Reserved     =  0;
    TdhGetProperty(ev, 0, NULL, 1, &dd, (ULONG) sizeof(uint32_t), (PBYTE) &value);
    return value;
}

/// @summary Retrieve an 8-bit signed integer property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The integer value.
internal_function int8_t
ProfilerGetSInt8
(
    EVENT_RECORD           *ev, 
    TRACE_EVENT_INFO *info_buf, 
    size_t               index
)
{
    PROPERTY_DATA_DESCRIPTOR dd;
    int8_t    value =  0;
    dd.PropertyName = (ULONGLONG)((uint8_t*) info_buf + info_buf->EventPropertyInfoArray[index].NameOffset);
    dd.ArrayIndex   =  ULONG_MAX;
    dd.Reserved     =  0;
    TdhGetProperty(ev, 0, NULL, 1, &dd, (ULONG) sizeof(int8_t), (PBYTE) &value);
    return value;
}

/// @summary Retrieve an ANSI string property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The NULL-terminated string buffer, or NULL. Free the returned buffer with the standard C library free function.
internal_function char*
ProfilerGetAnsiStr
(
    EVENT_RECORD           *ev, 
    TRACE_EVENT_INFO *info_buf, 
    size_t               index
)
{
    PROPERTY_DATA_DESCRIPTOR dd;
    ULONG prop_size =  0;
    char    *buffer =  NULL;
    dd.PropertyName = (ULONGLONG)((uint8_t*) info_buf + info_buf->EventPropertyInfoArray[index].NameOffset);
    dd.ArrayIndex   =  ULONG_MAX;
    dd.Reserved     =  0;
    if (TdhGetPropertySize(ev, 0, NULL, 1, &dd, &prop_size)  == ERROR_SUCCESS)
    {   // the size of the required buffer was retrieved successfully.
        if ((buffer =  (char*) malloc((size_t)   prop_size)) != NULL)
        {   // memory was successfully allocated for the string buffer.
            TdhGetProperty(ev, 0, NULL, 1, &dd,  prop_size, (PBYTE) buffer);
        }
    }
    return buffer;
}

/// @summary Retrieve a wide string property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The zero-terminated string buffer, or NULL. Free the returned buffer with the standard C library free function.
internal_function WCHAR*
ProfilerGetWideStr
(
    EVENT_RECORD           *ev, 
    TRACE_EVENT_INFO *info_buf, 
    size_t               index
)
{
    PROPERTY_DATA_DESCRIPTOR dd;
    ULONG prop_size =  0;
    WCHAR   *buffer =  NULL;
    dd.PropertyName = (ULONGLONG)((uint8_t*) info_buf + info_buf->EventPropertyInfoArray[index].NameOffset);
    dd.ArrayIndex   =  ULONG_MAX;
    dd.Reserved     =  0;
    if (TdhGetPropertySize(ev, 0, NULL, 1, &dd, &prop_size)  == ERROR_SUCCESS)
    {   // the size of the required buffer was retrieved successfully.
        if ((buffer = (WCHAR*) malloc((size_t)   prop_size)) != NULL)
        {   // memory was successfully allocated for the string buffer.
            TdhGetProperty(ev, 0, NULL, 1, &dd,  prop_size, (PBYTE) buffer);
        }
    }
    return buffer;
}

/// @summary Examine the ProviderGuid to determine whether an event was produced by the kernel logger.
/// @param info_buf A pointer to the event metadata.
/// @return true if the event was produced by the kernel trace logger.
internal_function inline bool
IsKernelTraceProvider
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->ProviderGuid, SystemTraceControlGuid) != FALSE;
}

/// @summary Examine the ProviderGuid to determine whether an event was produced by the task profiler.
/// @param info_buf A pointer to the event metadata.
/// @return true if the event was produced by the task profiler provider.
internal_function inline bool
IsTaskProfilerProvider
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->ProviderGuid, TaskProfilerProviderGuid) != FALSE;
}

/// @summary Examine the EventGuid to determine whether an event is a MOF Image_Load class event.
/// @param info_bu A pointer to the event metadata.
/// @return true if the event is a kernel trace image load or unload event.
internal_function inline bool
IsKernelImageLoadEvent
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->EventGuid, KernelImageLoadEventGuid) != FALSE;
}

/// @summary Examine the EventGuid to determine whether an event is a MOF Process class event.
/// @param info_buf A pointer to the event metadata.
/// @return true if the event is a kernel trace process event.
internal_function inline bool
IsKernelProcessEvent
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->EventGuid, KernelProcessEventGuid) != FALSE;
}

/// @summary Examine the EventGuid to determine whether an event is a MOF Thread class event.
/// @param info_buf A pointer to the event metadata.
/// @return true if the event is a kernel trace thread event.
internal_function inline bool
IsKernelThreadEvent
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->EventGuid, KernelThreadEventGuid) != FALSE;
}

/// @summary Examine the EventGuid to determine whether an event is a profiler performance info event.
/// @param info_buf A pointer to the event metadata.
/// @return true if the event is a kernel profiler performance info event.
internal_function inline bool
IsKernelPerfInfoEvent
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->EventGuid, KernelPerfInfoEventGuid) != FALSE;
}

/// @summary Examine the EventGuid to determine whether an event is a task profiler setup event.
/// @param info_buf A pointer to the event metadata.
/// @return true if the event represents a task profiler setup event.
internal_function inline bool
IsTaskProfilerSetupEvent
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->EventGuid, RegisterSchedulerComponentsEventGuid) != FALSE;
}

/// @summary Examine the EventGuid to determine whether an event is a TaskStateTransition event representing a task definition, ready-to-run, launch or finish transition.
/// @param info_buf A pointer to the event metadata.
/// @return true if the event represents a task state transition.
internal_function inline bool
IsTaskProfilerTaskTransitionEvent
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->EventGuid, TaskStateTransitionEventGuid) != FALSE;
}

internal_function void
ConsumeKernel_Thread_CSwitch
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{
    // this involves some ridiculous parsing of data in an opaque buffer.
    // there are some complications with context switch events:
    // 1. we probably don't know what the 'profiled' process and thread IDs are yet.
    //    - this means that all of the event info needs to be buffered.
    // 2. the ProcessId and ThreadId values in the event header are not valid.
    //    - need to look at Opcode 1,2,3,4 to retrieve thread start/stop information.
    // 
    if (IsEqualGUID(info_buf->ProviderGuid, SystemTraceControlGuid) && 
        IsEqualGUID(info_buf->EventGuid   , KernelThreadEventGuid)  && 
        info_buf->EventDescriptor.Opcode == 36)
    {   // opcode 36 corresponds to a CSwitch event.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744%28v=vs.85%29.aspx
        // all context switch event handling occurs on the same thread.
        WIN32_CONTEXT_SWITCH              cswitch;
        cswitch.CurrThreadId            = ProfilerGetUInt32(ev, info_buf,  0); // NewThreadId
        cswitch.PrevThreadId            = ProfilerGetUInt32(ev, info_buf,  1); // OldThreadId
        cswitch.WaitTimeMs              = ProfilerGetUInt32(ev, info_buf, 10); // NewThreadWaitTime
        cswitch.WaitReason              = ProfilerGetSInt8 (ev, info_buf,  6); // OldThreadWaitReason
        cswitch.WaitMode                = ProfilerGetSInt8 (ev, info_buf,  7); // OldThreadWaitMode
        cswitch.PrevThreadPriority      = ProfilerGetSInt8 (ev, info_buf,  3); // OldThreadPriority
        cswitch.CurrThreadPriority      = ProfilerGetSInt8 (ev, info_buf,  2); // NewThreadPriority
        profiler->CSwitchTime.push_back(uint64_t(ev->EventHeader.TimeStamp.QuadPart));
        profiler->CSwitchData.push_back(cswitch);
    }
}

/// @summary Decode the information from an event of type Process_TypeGroup1 and create or update a WIN32_PROCESS_INFO instance.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa364095(v=vs.85).aspx.
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @param terminate Specify true if the event specifies a process termination, or false if it indicates a process launch.
/// @return A pointer to the WIN32_PROCESS_INFO record that was updated.
internal_function WIN32_PROCESS_INFO*
ConsumeKernel_Process_TypeGroup1
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev, 
    bool                   terminate
)
{
}

internal_function WIN32_PROCESS_INFO*
FindProcessByPidAndTime
(
    WIN32_PROFILER_EVENTS *rtev, 
    uint32_t                pid, 
    uint64_t               time
)
{
    WIN32_PROCESS_LIST const   &list = rtev->ProcessList;
    for (size_t i = 0, n = list.ProcessCount; i < n; ++i)
    {
        if (list.ProcessId[i] == pid)
        {   // found a matching process ID, check the timestamp.
            if (time >= list.ProcessLifetime[i].CreateTime
    }
}

internal_function WIN32_PROCESS_INFO*
FindProcessByPidAndTime
(
    WIN32_PROFILER_EVENTS *rtev, 
    uint32_t                pid, 
    LONGLONG               time
)
{
}

// Other ConsumePROVIDER_EVENT functions here
// ... 

internal_function void
FilterKernelProcessEvent
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{
    if (info_buf->EventDescriptor.Opcode == 1 || info_buf->EventDescriptor.Opcode == 3)
    {   // information about a process that was just started, or was running when the trace started.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa364095(v=vs.85).aspx
    }
    if (info_buf->EventDescriptor.Opcode == 2)
    {   // information about a process that terminated while the trace was running.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa364095(v=vs.85).aspx
    }
}

internal_function void
FilterKernelThreadEvent
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{
    if (info_buf->EventDescriptor.Opcode == 50)
    {   // a ready thread event. very high frequency.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/dd765158%28v=vs.85%29.aspx
    }
    if (info_buf->EventDescriptor.Opcode == 36)
    {   // a context switch event. very high frequency.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744(v=vs.85).aspx
    }
    if (info_buf->EventDescriptor.Opcode ==  1 || info_buf->EventDescriptor.Opcode == 3)
    {   // information about a thread that was just started, or was running when the trace started.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/dd765166(v=vs.85).aspx
    }
    if (info_buf->EventDescriptor.Opcode == 2)
    {   // information about a thread that terminated while the trace was running.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/dd765166(v=vs.85).aspx
    }
}

internal_function void
FilterKernelImageLoadEvent
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{
}

internal_funfction void
FilterKernelProviderEvent
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   // the following statements are ordered by event frequency, highest to lowest.
    if      (IsKernelThreadEvent   (info_buf)) FilterKernelThreadEvent   (rtev, info_buf, info_size, ev);
    else if (IsKernelImageLoadEvent(info_buf)) FilterKernelImageLoadEvent(rtev, info_buf, info_size, ev);
    else if (IsKernelProcessEvent  (info_buf)) FilterKernelProcessEvent  (rtev, info_buf, info_size, ev);
    // else, the profiler doesn't currently care about this class of kernel trace event.
}

internal_funfction void
FilterTaskProfilerEvent
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   // the following statements are ordered by event frequency, highest to lowest.
    // ...
    // else, the profiler doesn't currently care about this class of task profiler event.
}

/// @summary Callback invoked for each event reported by Event Tracing for Windows.
/// @param ev Data associated with the event being reported.
internal_function void WINAPI
ProfilerRecordEvent
(
    EVENT_RECORD *ev
)
{
    WIN32_PROFILER_EVENTS *profiler = (WIN32_PROFILER_EVENTS*) ev->UserContext;
    TRACE_EVENT_INFO      *info_buf = (TRACE_EVENT_INFO*) profiler->EventBuffer;
    ULONG                  size_buf = (ULONG) profiler->EventBufferSize;

    // retrieve event metadata used to dispatch the event for processing.
    if (TdhGetEventInformation(ev, 0, NULL, info_buf, &size_buf) == ERROR_SUCCESS)
    {   // event metadata was successfully retrieved; dispatch the event 
        // based on the system that produced it.
        if      (IsKernelTraceProvider (info_buf)) FilterKernelProviderEvent(profiler, info_buf, size_buf, ev);
        else if (IsTaskProfilerProvider(info_buf)) FilterTaskProfilerEvent  (profiler, info_buf, size_buf, ev);
        // else, the profiler doesn't currently use events from the provider. drop it.
    }
    else
    {   // TODO(rlk): probably the buffer size is too small.
        // handle the case where TdhGetEventInformation fails.
    }
}

/// @summary Implements the entry point of the thread that dispatches ETW context switch events.
/// @param argp A pointer to the WIN32_TASK_PROFILER to which events will be logged.
/// @return Zero (unused).
internal_function unsigned int __stdcall
EventConsumerThreadMain
(
    void *argp
)
{   // wait for the go signal from the main thread.
    // this ensures that state is properly set up.
    DWORD               wait_result =  WAIT_OBJECT_0;
    WIN32_PROFILER_EVENTS *profiler = (WIN32_PROFILER_EVENTS*) argp;
    if ((wait_result = WaitForSingleObject(profiler->ConsumerLaunch, INFINITE)) != WAIT_OBJECT_0)
    {   // the wait failed for some reason, so terminate early.
        ConsoleError("ERROR (%S): Event consumer wait for launch failed with result %08X (%08X).\n", __FUNCTION__, wait_result, GetLastError());
        return 1;
    }

    // because real-time event capture is being used, the ProcessTrace function 
    // doesn't return until the capture session is stopped by calling CloseTrace.
    // ProcessTrace sorts events by timestamp and calls TaskProfilerRecordEvent.
    ULONG result  = ProcessTrace(&profiler->ConsumerHandle, 1, NULL, NULL);
    if   (result != ERROR_SUCCESS)
    {   // the thread is going to terminate because an error occurred.
        ConsoleError("ERROR (%S): Context switch consumer terminating with result %08X.\n", __FUNCTION__, result);
        return 1;
    }

    // all events have been consumed, so close the trace session.
    CloseHandle(profiler->ConsumerLaunch); profiler->ConsumerLaunch = NULL;
    CloseTrace(profiler->ConsumerHandle); profiler->ConsumerHandle = NULL;
    return 0;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Allocate resources for a new WIN32_PROFILER_EVENTS container, open a trace session and consume the event data.
/// @param ev The profiler events container to initialize.
/// @param trace_file A NULL-terminated string specifying the path of the file to load.
public_function int
NewProfilerEvents
(
    WIN32_PROFILER_EVENTS         *ev, 
    TCHAR const           *trace_file
)
{   
    TRACEHANDLE           trace = INVALID_PROCESSTRACE_HANDLE;
    EVENT_TRACE_LOGFILE logfile = {};
    HANDLE               thread = NULL;
    unsigned int            tid = 0;

    // initialize the fields of the events structure.
    ev->EventBuffer      = NULL;
    ev->EventBufferSize  = 0;
    ev->ConsumerHandle   = INVALID_PROCESSTRACE_HANDLE;
    ev->ConsumerLaunch   = CreateEvent(NULL, TRUE, FALSE, NULL); // manual-reset
    ev->ConsumerThread   = NULL;
    ev->ConsumerThreadId = 0;

    // attempt to open the trace file from the supplied path.
    logfile.LogFileName         = (TCHAR*)trace_file;
    logfile.LoggerName          = NULL;
    logfile.ProcessTraceMode    = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    logfile.EventRecordCallback = ProfilerRecordEvent;
    logfile.Context             = ev;
    if ((trace = OpenTrace(&logfile)) == INVALID_PROCESSTRACE_HANDLE)
    {   // if the trace cannot be opened, there's no point in continuing.
        ConsoleError("ERROR (%S): Unable to open the trace session (%08X).\n", __FUNCTION__, GetLastError());
        CloseHandle(ev->ConsumerLaunch); ev->ConsumerLaunch = NULL;
        return -1;
    }

    // start a background thread to receive events read from the trace file.
    if ((thread = (HANDLE)_beginthreadex(NULL, 0, EventConsumerThreadMain, ev, 0, &tid)) == NULL)
    {   // without the background thread, the profiler can't receive the events.
        ConsoleError("ERROR (%S): Unable to start event consumer thread (errno = %d).\n", __FUNCTION__, errno);
        CloseHandle(ev->ConsumerLaunch); ev->ConsumerLaunch = NULL;
        CloseTrace(trace);
        return -1;
    }

    // TODO(rlk): might want to move to a memory arena system based around VirtualAlloc.
    // this would provide better performance and probably lead to less memory waste.
    ev->EventBuffer      = (uint8_t*) malloc(64 * 1024); // 64KB
    ev->EventBufferSize  = 64 * 1024;
    ev->ConsumerHandle   = trace;
    ev->ConsumerThread   = thread;
    ev->ConsumerThreadId = tid;

    // initialize the storage for the context switch track. context switch events
    // occur at an extremely high frequency (10000+/core/second).
    ev->CSwitchTime.clear(); ev->CSwitchTime.reserve(1000000);
    ev->CSwitchData.clear(); ev->CSwitchData.reserve(1000000);

    // TODO(rlk): initialize other event data containers here.
    // ...

    // start populating our data structures with event data from the trace file.
    SetEvent(ev->ConsumerLaunch);
    return 0;
}

