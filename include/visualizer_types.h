/*/////////////////////////////////////////////////////////////////////////////
/// @summary Define the data types used by the visualizer to represent 
/// profiling data. Data is designed for efficient searching.
///////////////////////////////////////////////////////////////////////////80*/

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Define the representation of a task handle within the scheduler.
typedef uint32_t task_id_t;        /// Tasks are referred to by a 32-bit handle value.

/// @summary Defines the data associated with a context switch event. This data is extracted from the EVENT_RECORD::UserData field.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa364115%28v=vs.85%29.aspx
/// See https://msdn.microsoft.com/en-us/library/aa964744%28VS.85%29.aspx
struct WIN32_CONTEXT_SWITCH
{
    uint32_t   PrevThreadId;       /// The system identifier of the thread being switched out.
    uint32_t   CurrThreadId;       /// The system identifier of the thread being switched in.
    uint32_t   WaitTimeMs;         /// The number of milliseconds that the CurrThreadId thread spent in a wait state.
    int8_t     WaitReason;         /// The reason the switch-out thread is being placed into a wait state.
    int8_t     WaitMode;           /// Whether the switched-out thread has entered a kernel-mode wait (0) or a user-mode wait (1).
    int8_t     PrevThreadPriority; /// The priority value of the thread being switched out.
    int8_t     CurrThreadPriority; /// The priority value of the thread being switched in.
};

/// @summary Define the data used to locate a task definition at a specific time.
struct WIN32_TASK_AND_TIME
{
    uint64_t   Timestamp;          /// The timestamp value (in ticks) at which the event occurred.
    task_id_t  TaskId;             /// The task identifier.
};

/// @summary Define the data for all profiler events the visualizer cares about. This is the top-level data object.
struct WIN32_PROFILER_EVENTS
{
    uint8_t                            *EventBuffer;       /// A buffer used for parsing event data from the trace.
    size_t                              EventBufferSize;   /// The size of the event parsing buffer, in bytes.

    TRACEHANDLE                         ConsumerHandle;    /// The ETW trace handle returned by OpenTrace.
    HANDLE                              ConsumerLaunch;    /// A manual-reset event used by the consumer thread to signal the start of the trace session.
    HANDLE                              ConsumerThread;    /// The handle of the thread used to receive events from the trace session.
    unsigned int                        ConsumerThreadId;  /// The operating system identifier of the event consumer thread.

    std::vector<uint64_t>               CSwitchTime;       /// The timestamp value associated with each context switch event.
    std::vector<WIN32_CONTEXT_SWITCH>   CSwitchData;       /// The data associated with each context switch event.
};

/*////////////////////////
//   Public Functions   //
////////////////////////*/

