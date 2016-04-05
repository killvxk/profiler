/*/////////////////////////////////////////////////////////////////////////////
/// @summary Define the data types used by the visualizer to represent 
/// profiling data. Data is designed for efficient searching.
///////////////////////////////////////////////////////////////////////////80*/

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Define the representation of a task handle within the scheduler.
typedef uint32_t task_id_t;                                 /// Tasks are referred to by a 32-bit handle value.

/// @summary Defines the data associated with a thread being switched in by the scheduler.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744%28v=vs.85%29.aspx
struct WIN32_SWITCH_IN_DATA
{
    uint32_t                            WaitTime;           /// The amount of time the thread spent waiting, in milliseconds.
    int8_t                              Processor;          /// The zero-based index of the processor the thread is running on.
    int8_t                              Priority;           /// The scheduler priority of the thread when it was switched in.
};

/// @summary Defines the data associated with a thread being switched out by the scheduler.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744%28v=vs.85%29.aspx
struct WIN32_SWITCH_OUT_DATA
{
    int8_t                              Processor;          /// The zero-based index of the processor the thread was running on?
    int8_t                              State;              /// The thread state when it was switched out.
    int8_t                              WaitReason;         /// The reason the thread is being placed into a wait state.
    int8_t                              WaitMode;           /// Whether the thread is being placed into a user-mode or kernel-mode wait state.
    int8_t                              Priority;           /// The scheduler priority of the thread when it was switched out.
};

/// @summary Defines the data associated with the lifetime of an image, process or thread.
struct WIN32_LIFETIME
{
    uint64_t                            CreateTime;         /// The timestamp value (in nanoseconds) at which the process or thread was created, or the image was loaded.
    uint64_t                            DestroyTime;        /// The timestamp value (in nanoseconds) at which the process or thread was destroyed, or the image was unloaded.
};

/// @summary Defines the 
struct WIN32_IMAGE_INFO
{
    WCHAR                              *ImagePath;          /// A zero-terminated string specifying the path of the image file.
};

/// @summary Defines the data associated with each thread that existed at some point during a process lifetime.
struct WIN32_THREAD_INFO
{
    uint32_t                            ThreadId;           /// The operating system thread identifier.
    uint64_t                            EntryAddress;       /// The address of the thread entry point relative to the image base address.
    WCHAR                              *EntryPointName;     /// A zero-terminated string corresponding to the entry point symbol.
    size_t                              ReadyCount;         /// The number of times the thread was readied by the scheduler.
    std::vector<uint64_t>               ReadyTimes;         /// The timestamp (in nanoseconds) at which the thread was readied by the scheduler.
    size_t                              SwitchInCount;      /// The number of times the thread was switched in.
    std::vector<uint64_t>               SwitchInTime;       /// The timestamp (in nanoseconds) at which the thread was switched in by the scheduler.
    std::vector<WIN32_SWITCH_IN_DATA>   SwitchInData;       /// Additional information associated with the thread activation.
    size_t                              SwitchOutCount;     /// The number of times the thread was switched out.
    std::vector<uint64_t>               SwitchOutTime;      /// The timestamp (in nanoseconds) at which the thread was switched out by the scheduler.
    std::vector<WIN32_SWITCH_OUT_DATA>  SwitchOutData;      /// Additional information associated with the thread deactivation.
};

/// @summary Defines the data associated with each process that existed at some point during the trace capture.
struct WIN32_PROCESS_INFO
{
    uint32_t                            ProcessId;          /// The operating system process identifier.
    uint32_t                            Reserved;           /// Reserved for future use. Set to 0.
    WCHAR                              *Executable;         /// The path of the process executable image.
    size_t                              ThreadCount;        /// The number of threads created during the process lifetime.
    std::vector<uint32_t>               ThreadId;           /// The operating system identifier for each thread that existed at some point during the process lifetime.
    std::vector<WIN32_LIFETIME>         ThreadLifetime;     /// The creation and destruction time for each thread that existed at some point during the process lifetime.
    std::vector<WIN32_THREAD_INFO>      ThreadInfo;         /// Additional information about each thread that existed at some point during the process lifetime.
    size_t                              ImageCount;         /// The number of executable images loaded into the process address space.
    std::vector<uint64_t>               ImageBaseAddress;   /// The base load address for each image that existed at some point during the process lifetime.
    std::vector<uint32_t>               ImagePathHash;      /// The 32-bit hash of the file path for each image that existed at some point during the process lifetime.
    std::vector<WIN32_LIFETIME>         ImageLifetime;      /// The load and unload time for each image that existed at some point during the process lifetime.
    std::vector<WIN32_IMAGE_INFO>       ImageInfo;          /// Additional information about each image that existed at some point during the process lifetime.
};

/// @summary Defines the data associated with the list of processes that have produced events in the trace.
struct WIN32_PROCESS_LIST
{
    size_t                              ProcessCount;       /// The number of processes defined in the process list.
    std::vector<uint32_t>               ProcessId;          /// The operating system identifier for each process in the list.
    std::vector<uint32_t>               ProcessHash;        /// The 32-bit hash value of the image path for each process.
    std::vector<WIN32_LIFETIME>         ProcessLifetime;    /// The creation and destruction time for each process in the list.
    std::vector<WIN32_PROCESS_INFO>     ProcessInfo;        /// Additional information about each process in the list.
};

/// @summary Define the data used to locate a task definition at a specific time.
struct WIN32_TASK_AND_TIME
{
    uint64_t                            Timestamp;          /// The timestamp value (in ticks) at which the event occurred.
    task_id_t                           TaskId;             /// The task identifier.
};

/// @summary Define the data for all profiler events the visualizer cares about. This is the top-level data object.
struct WIN32_PROFILER_EVENTS
{
    uint8_t                            *EventBuffer;        /// A buffer used for parsing event data from the trace.
    size_t                              EventBufferSize;    /// The size of the event parsing buffer, in bytes.

    TRACEHANDLE                         ConsumerHandle;     /// The ETW trace handle returned by OpenTrace.
    HANDLE                              ConsumerLaunch;     /// A manual-reset event used by the consumer thread to signal the start of the trace session.
    HANDLE                              ConsumerThread;     /// The handle of the thread used to receive events from the trace session.
    unsigned int                        ConsumerThreadId;   /// The operating system identifier of the event consumer thread.

    size_t                              PointerSize;        /// The PointerSize field of the EVENT_TRACE_LOGFILE::LogfileHeader, specifying the size of pointer values on the producer, in bytes.
    uint64_t                            TimerResolution;    /// The TimerResolution field of the EVENT_TRACE_LOGFILE::LogfileHeader specifying the producer hardware timer resolution in 100-nanosecond units.
    LARGE_INTEGER                       ClockFrequency;     /// The PerfFreq field of the EVENT_TRACE_LOGFILE::LogfileHeader specifying the high-resolution timer counts-per-second on the producer.
    WIN32_PROCESS_LIST                  ProcessList;        /// The list of information about all processes that were active during the trace.
};

/*////////////////////////
//   Public Functions   //
////////////////////////*/

