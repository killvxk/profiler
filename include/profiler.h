/*/////////////////////////////////////////////////////////////////////////////
/// @summary Declare the public interface to the profiler.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////////
//   Preprocessor   //
////////////////////*/
/// @summary Specify ENABLE_PROFILER as 1 to include calls to the profiler.
/// Specify ENABLE_PROFILER as 0 to strip out all calls to the profiler.
#ifndef ENABLE_PROFILER
#define ENABLE_PROFILER           0
#endif

/// @summaey Define the major version of the profiler. The major version increments when a breaking API change is introduced.
#ifndef PROFILER_VERSION_MAJOR
#define PROFILER_VERSION_MAJOR    1
#endif

/// @summary Define the minor version of the profiler. The minor version increments when a backwards-compatible API change is introduced.
#ifndef PROFILER_VERSION_MINOR
#define PROFILER_VERSION_MINOR    0
#endif

/// @summary Define the constant used to indicate an invalid or unused task identifier.
#ifndef INVALID_TASK_ID
#define INVALID_TASK_ID           0x7FFFFFFFUL
#endif

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Define the result codes that can be returned from the InitializeProfiler call.
enum PROFILER_RESULT : int32_t
{
    PROFILER_RESULT_SUCCESS         = 0, /// The profiler was successfully initialized.
    PROFILER_RESULT_INVALID_VERSION = 1, /// The profiler does not support the requested API version.
    PROFILER_RESULT_INVALID_APPINFO = 2, /// The application information supplied in the PROFILER_CONFIG is not valid.
};

/// @summary Define the configuration information passed by the application to the profiler.
struct PROFILER_CONFIG
{
    char const *ApplicationName;         /// A NULL-terminated ANSI string identifying the application to the profiler.
    uint32_t    ApplicationMajorVersion; /// The major version of the application.
    uint32_t    ApplicationMinorVersion; /// The minor version of the application.
    uint32_t    ProfilerMajorVersion;    /// The major version of the profiler API the application is built against.
    uint32_t    ProfilerMinorVersion;    /// The minor version of the profiler API the application is built against.
    uint32_t    ComputePoolSize;         /// The maximum number of worker threads in the application compute thread pool.
    uint32_t    GeneralPoolSize;         /// The maximum number of worker threads in the application general thread pool.
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary The MOF class Image event GUID (ImageLoadGuid). The GUID is {2cb15d1d-5fc1-11d2-abe1-00a0c911f518}.
/// See NT Kernel Logger Constants: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364085(v=vs.85).aspx
DEFINE_GUID(
    KernelImageLoadEventGuid,
    0x2cb15d1d,
    0x5fc1,
    0x11d2,
    0xab, 0xe1, 0x00, 0xa0, 0xc9, 0x11, 0xf5, 0x18);

/// @summary The MOF class Process event GUID (ProcessGuid). The GUID is {3d6fa8d0-fe05-11d0-9dda-00c04fd7ba7c}.
/// See NT Kernel Logger Constants: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364085(v=vs.85).aspx
DEFINE_GUID(
    KernelProcessEventGuid,
    0x3d6fa8d0,
    0xfe05,
    0x11d0,
    0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c);

/// @summary The MOF class Thread event GUID (ThreadGuid). The GUID is {3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c}.
/// See NT Kernel Logger Constants: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364085(v=vs.85).aspx
DEFINE_GUID(
    KernelThreadEventGuid,
    0x3d6fa8d1,
    0xfe05,
    0x11d0,
    0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c);

/// @summary The MOF class PerfInfo event GUID (PerfInfoGuid). The GUID is {ce1dbfb4-137e-4da6-87b0-3f59aa102cbc}.
/// See NT Kernel Logger Constants: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364085(v=vs.85).aspx
DEFINE_GUID(
    KernelPerfInfoEventGuid,
    0xce1dbfb4,
    0x137e,
    0x4da6,
    0x87, 0xb0, 0x3f, 0x59, 0xaa, 0x10, 0x2c, 0xbc);

/// @summary The Profiler.Task provider GUID. The GUID is {042cd377-8f6e-4Bf0-93de-b4ba32234771}.
DEFINE_GUID(
    TaskProfilerProviderGuid, 
    0x042cd377, 
    0x8f6e,
    0x4bf0, 
    0x93, 0xde, 0xb4, 0xba, 0x32, 0x23, 0x47, 0x71);

/// @summary The RegisterSchedulerComponents event GUID. The GUID is {B4C458C7-AD6A-494C-9517-159821F304BE}.
DEFINE_GUID(
    RegisterSchedulerComponentsEventGuid, 
    0xb4c458c7, 
    0xad6a, 
    0x494c,
    0x95, 0x17, 0x15, 0x98, 0x21, 0xf3, 0x04, 0xbe);

/// @summary The TaskStateTransition event GUID. The GUID is {249C14B6-FEE0-4797-930F-2B08389A3EFD}.
DEFINE_GUID(
    TaskStateTransitionEventGuid, 
    0x249c14b6, 
    0xfee0, 
    0x4797, 
    0x93, 0x0f, 0x2b, 0x08, 0x38, 0x9a, 0x3e, 0xfd);

/*//////////////////////////
//   Internal Functions   //
//////////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
#if ENABLE_PROFILER
/// @summary Initialize the profiler and register basic application properties.
/// @param config An object describing the application to the profiler.
/// @return One of PROFILER_RESULT specifying whether the profiler was successfully initialized.
extern int32_t __cdecl
InitializeProfiler
(
    PROFILER_CONFIG *config
);

/// @summary Shutdown the profiler prior to application shutdown.
extern void __cdecl
ShowdownProfiler
(
    void
);

/// @summary Register information about a worker thread with the profiler.
/// @param thread_id The operating system identifier of the worker thread.
/// @param pool The application identifier of the thread pool.
/// @param pool_index The zero-based index of the worker thread within the pool.
extern void __cdecl
RegisterWorkerThread
(
    uint32_t  thread_id,
    uint32_t       pool, 
    uint32_t pool_index
);

/// @summary Register information about a thread that can produce tasks with the profiler.
/// @param source_name A NULL-terminated ANSI string identifying the source thread.
/// @param owning_thread_id The operating system identifier of the task producer thread.
/// @param source_index The zero-based index of the task source within the scheduler.
extern void __cdecl
RegisterTaskSource
(
    char const      *source_name, 
    uint32_t    owning_thread_id,
    uint32_t        source_index
);

/// @summary Mark the point in time at which a task is defined.
/// @param task_id The identifier of the new task.
/// @param parent_id The identifier of the parent task, or INVALID_TASK_ID.
/// @param task_main The entry point of the task.
/// @param source_index The zero-based index of the task source within the scheduler.
/// @param dependency_count The number of tasks that must complete before the new task can run.
/// @param dependencies The list of task identifiers that must complete before the new task can run, or NULL.
extern void __cdecl
MarkTaskDefinition
(
    uint32_t             task_id, 
    uint32_t           parent_id, 
    void              *task_main, 
    uint32_t        source_index, 
    uint32_t    dependency_count, 
    uint32_t const *dependencies
);

/// @summary Mark the point in time at which a task becomes ready-to-run.
/// @param task_id The identifier of the task that is now ready-to-run.
/// @param source_index The zero-based index of the task source within the scheduler that's responsible for the state transition.
extern void __cdecl
MarkTaskReadyToRun
(
    uint32_t      task_id, 
    uint32_t source_index
);

/// @summary Mark the point in time at which a worker thread begins executing a task.
/// @param task_id The identifier of the task that is being launched.
extern void __cdecl
MarkTaskLaunch
(
    uint32_t task_id
);

/// @summary Mark the point in time at which a worker thread finishes executing a task.
/// @param task_id The identifier of the task that is being launched.
extern void __cdecl
MarkTaskFinish
(
    uint32_t task_id
);
#else /* the profiler is disabled */
#define InitializeProfiler(config)        1
#define ShutdownProfiler                  
#define RegisterWorkerThread              
#define RegisterTaskSource                
#define MarkTaskDefinition
#define MarkTaskReadyToRun                
#define MarkTaskLaunch                    
#define MarkTaskFinish                    
#endif

