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
/// @summary Mix the bits of a 32-bit unsigned integer. Used for hash generation.
/// @param x The 32-bit unsigned integer to mix.
/// @return The input value, with bits mixed.
public_function inline uint32_t 
BitMixU32
(
    uint32_t x
)
{   // the finalizer from murmurhash3, 32-bit.
    x ^= x >> 16;
    x *= 0x85EBCA6B;
    x ^= x >> 13;
    x *= 0xC2B2AE35;
    x ^= x >> 16;
    return x;
}

/// @summary Compute a hash value for a zero-terminated wide character string.
/// @param stringbuf A zero-terminated wide character string.
/// @return The 32-bit hash of the string.
public_function uint32_t
HashWideString
(
    WCHAR *stringbuf
)
{
    uint32_t hash  = 0;
    if (stringbuf != NULL)
    {
        while (*stringbuf)
        {
            hash = _rotl(*stringbuf++, 7) + hash;
        }
    }
    return BitMixU32(hash);
}

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

/// @summary Convert an event timestamp from ticks to nanoseconds.
/// @param ev The event record.
/// @param frequency The high-resolution timer frequency of the system that produced the trace, in ticks-per-second.
public_function inline uint64_t
EventTimeToNanoseconds
(
    EVENT_RECORD        *ev, 
    LARGE_INTEGER frequency
)
{
    uint64_t  timestamp = uint64_t(ev->EventHeader.TimeStamp.QuadPart);
    uint64_t clock_freq = uint64_t(frequency.QuadPart);
    return (1000000000ULL * timestamp) / clock_freq;
}

/// @summary Find the range of events that started or stopped within a given time range.
/// @param event_times An array of timestamp values, sorted into ascending order, and possibly containing duplicates, where each timestamp corresponds to an event. 
/// @param event_count The number of elements in the array of event times.
/// @param range_lower The start of the search interval, in nanoseconds.
/// @param range_upper The end of the search interval, in nanoseconds.
/// @param index_lower On return, this value is set to the zero-based index of the oldest event in the time range.
/// @param index_upper On return, this value is set to the zero-based index of the newest event in the time range.
/// @param output_count On return, this value is set to the the number of events in the time range.
/// @return true if at least one event occurred within the time range.
public_function bool
FindEventsInTimeRange
(
    uint64_t const *event_times, 
    size_t   const  event_count,
    uint64_t        range_lower, 
    uint64_t        range_upper, 
    size_t         &index_lower, 
    size_t         &index_upper,
    size_t        &output_count
)
{
    if (event_count == 0)
    {   // early out - the event set is empty.
        output_count = 0;
        return false;
    }
    if (range_upper  < event_times[0])
    {   // early out - the time range occurs prior to the start of the event set.
        output_count = 0;
        return false;
    }
    if (range_lower  > event_times[event_count-1])
    {   // early out - the time range starts after the end of the event set.
        output_count = 0;
        return false;
    }
    if (range_lower <= event_times[0])
    {   // early out for lower bound, which occurs prior to the start of the event set.
        index_lower  = 0;
    }
    else
    {   // perform a binary search for the first value less than the lower bound, 
        // where the following value is greater than or equal to the lower bound.
        intptr_t   m;
        intptr_t   l = -1;
        intptr_t   r = intptr_t(event_count) - 1;
        while (r - l > 1)
        {   // calculate the midpoint of the search interval.
            m = l + (r - l) / 2;
            // check the time value at the midpoint.
            if (event_times[m] >= range_lower)
            {   // move the upper-bound search index closer to the start of the search space.
                r = m;
            }
            else
            {   // move the lower-bound search index closer to the end of the search space.
                l = m;
            }
        }
        index_lower = size_t(r);
    }
    if (range_upper >= event_times[event_count-1])
    {   // early out for upper bound, which occurs after the end of the event set.
        index_upper  = event_count - 1;
    }
    else
    {   // perform a binary search for the first value greater than the upper bound, 
        // where the previous value is less than or equal to the upper bound.
        intptr_t   m;
        intptr_t   l = 0;
        intptr_t   r = intptr_t(event_count);
        while (r - l > 1)
        {   // calculate the midpoint of the search interval.
            m = l + (r - l) / 2;
            // check the time value at the midpoint.
            if (event_times[m] <= range_upper)
            {   // move the lower-bound search index closer to the end of the search space.
                l = m;
            }
            else
            {   // move the upper-bound search index closer to the start of the search space.
                r = m;
            }
        }
        index_upper = size_t(l);
    }

    output_count = (index_upper - index_lower) + 1;
    return true;
}

/// @summary Search an object list (process, thread, etc.) for an object with a given identifier that was alive at a particular time.
/// @param key_list The list of object identifiers to search. The list may contain duplicates.
/// @param lifetime_list The list of object lifetimes corresponding to each identifier.
/// @param object_count The number of values in the key and lifetime lists.
/// @param search_key The identifier of the object to locate.
/// @param search_time The query time, in nanoseconds. The object must be alive at this time.
/// @param object_index If the function returns true, this value is set to the zero-based index of the object in the object list.
/// @return true if an object with the specified identifier was alive and located at the specified time, or false otherwise.
public_function bool
FindObjectByU32AndTime
(
    uint32_t       const      *key_list,
    WIN32_LIFETIME const *lifetime_list, 
    size_t         const   object_count,
    uint32_t                 search_key, 
    uint64_t                search_time, 
    size_t                &object_index
)
{
    for (size_t i = 0; i < object_count; ++i)
    {
        if (key_list[i] == search_key)
        {   // found a key match, check lifetime.
            if (ObjectAliveAtTime(lifetime_list[i], search_time))
            {   // the object was also alive, so we have a match.
                object_index = i;
                return true;
            }
        }
    }
    return false;
}

/// @summary Search an object list (process, thread, etc.) for an object with a given identifier that was alive at a particular time.
/// @param key_list The list of object identifiers to search. The list may contain duplicates.
/// @param lifetime_list The list of object lifetimes corresponding to each identifier.
/// @param object_count The number of values in the key and lifetime lists.
/// @param search_key The identifier of the object to locate.
/// @param search_time The query time, in nanoseconds. The object must be alive at this time.
/// @param object_index If the function returns true, this value is set to the zero-based index of the object in the object list.
/// @return true if an object with the specified identifier was alive and located at the specified time, or false otherwise.
public_function bool
FindObjectByU64AndTime
(
    uint64_t       const      *key_list,
    WIN32_LIFETIME const *lifetime_list, 
    size_t         const   object_count,
    uint64_t                 search_key, 
    uint64_t                search_time, 
    size_t                &object_index
)
{
    for (size_t i = 0; i < object_count; ++i)
    {
        if (key_list[i] == search_key)
        {   // found a key match, check lifetime.
            if (ObjectAliveAtTime(lifetime_list[i], search_time))
            {   // the object was also alive, so we have a match.
                object_index = i;
                return true;
            }
        }
    }
    return false;
}

/// @summary Search the process list of a profiler events list for a system process identifier.
/// @param rtev The runtime profiler events record.
/// @param pid The process identifier to search for.
/// @param time The nanosecond timestamp at which the event occurred.
/// @param index If the function returns true, this value is set to the zero-based index of the process record in the process list.
/// @return true if a process with the specified process ID was alive and located at the specified time, or false if no record exists.
public_function inline bool
FindProcessByPid
(
    WIN32_PROFILER_EVENTS *rtev, 
    uint32_t                pid, 
    uint64_t               time, 
    size_t               &index
)
{
    uint32_t       const       *key_list = &rtev->ProcessList.ProcessId[0];
    WIN32_LIFETIME const  *lifetime_list = &rtev->ProcessList.ProcessLifetime[0];
    return FindObjectByU32AndTime(key_list, lifetime_list, rtev->ProcessList.ProcessCount, pid, time, index);
}

/// @summary Search the process list of a profiler events list for a process identified by path.
/// @param rtev The runtime profiler events record.
/// @param hash The 32-bit hash value of the process name.
/// @param time The nanosecond timestamp at which the event occurred.
/// @param index If the function returns true, this value is set to the zero-based index of the process record in the process list.
/// @return true if a process with the specified process ID was alive and located at the specified time, or false if no record exists.
public_function inline bool
FindProcessByName
(
    WIN32_PROFILER_EVENTS *rtev, 
    uint32_t               hash, 
    uint64_t               time, 
    size_t               &index
)
{
    uint32_t       const       *key_list = &rtev->ProcessList.ProcessHash[0];
    WIN32_LIFETIME const  *lifetime_list = &rtev->ProcessList.ProcessLifetime[0];
    return FindObjectByU32AndTime(key_list, lifetime_list, rtev->ProcessList.ProcessCount, hash, time, index);
}

/// @summary Find a process record for the process with the specified identifier. Create a new record if no existing record is found. 
/// @param rtev The runtime profiler events record to search or update.
/// @param pid The process identifier to search for.
/// @param time The nanosecond timestamp at which the event occurred.
/// @return The zero-based index of the process record within the trace process list.
public_function size_t
FindOrCreateProcess
(
    WIN32_PROFILER_EVENTS *rtev, 
    uint32_t                pid, 
    uint64_t               time
)
{
    size_t process_index;
    if (FindProcessByPid(rtev, pid, time, process_index))
    {   // return the index of the existing record.
        return process_index;
    }
    else
    {   // insert a new, empty record in the process list.
        WIN32_LIFETIME         lifetime;
        WIN32_PROCESS_INFO process_info;
        process_info.ProcessId    = pid;
        process_info.Reserved     = 0;
        process_info.Executable   = NULL;
        process_info.ThreadCount  = 0;
        process_info.ImageCount   = 0;
        InitObjectLifetime(lifetime, time);
        process_index = rtev->ProcessList.ProcessCount++;
        rtev->ProcessList.ProcessId.push_back(pid);
        rtev->ProcessList.ProcessHash.push_back(0);
        rtev->ProcessList.ProcessLifetime.push_back(lifetime);
        rtev->ProcessList.ProcessInfo.push_back(process_info);
        return process_index;
    }
}

/// @summary Search the thread list of a process for a thread given the thread ID.
/// @param process The process record to search.
/// @param tid The operating system identifier of the thread.
/// @param time The nanosecond timestamp at which the event occurred.
/// @param index If the function returns true, this value is set to the zero-based index of the thread record in the process thread list.
/// @return true if a thread with the specified identifier was alive and located at the specified time, or false if no record exists.
public_function bool
FindThreadByTid
(
    WIN32_PROCESS_INFO *process, 
    uint32_t                tid, 
    uint64_t               time, 
    size_t               &index
)
{
    uint32_t       const      *key_list = &process->ThreadId[0];
    WIN32_LIFETIME const *lifetime_list = &process->ThreadLifetime[0];
    return FindObjectByU32AndTime(key_list, lifetime_list, process->ThreadCount, tid, time, index);
}

/// @summary Find a record for the thread with the specified ID. Create a new record if no existing record is found. 
/// @param process The process record to search.
/// @param tid The operating system identifier of the thread.
/// @param entry The address of the thread entry point in the process address space.
/// @param time The nanosecond timestamp at which the event occurred.
/// @return The zero-based index of the thread record within the process thread list.
public_function size_t
FindOrCreateThread
(
    WIN32_PROCESS_INFO *process, 
    uint32_t                tid,
    uint64_t              entry, 
    uint64_t               time
)
{
    size_t thread_index;
    if (FindThreadByTid(process, tid, time, thread_index))
    {   // return the index of the existing record.
        return thread_index;
    }
    else
    {   // insert a new, empty record in the process thread list.
        WIN32_LIFETIME       lifetime;
        WIN32_THREAD_INFO thread_info;
        thread_info.ThreadId        = tid;
        thread_info.EntryAddress    = entry;
        thread_info.EntryPointName  = NULL;
        thread_info.ReadyCount      = 0;
        thread_info.SwitchInCount   = 0;
        thread_info.SwitchOutCount  = 0;
        InitObjectLifetime(lifetime, time);
        thread_index = process->ThreadCount++;
        process->ThreadId.push_back(tid);
        process->ThreadLifetime.push_back(lifetime);
        process->ThreadInfo.push_back(thread_info);
        return thread_index;
    }
}

/// @summary Search the image list of a process for an executable image path.
/// @param process The process record to search.
/// @param hash The 32-bit hash value of the executable image path.
/// @param time The nanosecond timestamp at which the event occurred.
/// @param index If the function returns true, this value is set to the zero-based index of the image record in the process image list.
/// @return true if an image with the specified path was alive and located at the specified time, or false if no record exists.
public_function bool
FindImageByName
(
    WIN32_PROCESS_INFO *process, 
    uint32_t               hash,
    uint64_t               time, 
    size_t               &index
)
{
    uint32_t       const      *key_list  = &process->ImagePathHash[0];
    WIN32_LIFETIME const *lifetime_list  = &process->ImageLifetime[0];
    return FindObjectByU32AndTime(key_list, lifetime_list, process->ImageCount, hash, time, index);
}

/// @summary Search the image list of a process for an executable image given the base load address.
/// @param process The process record to search.
/// @param addr The base load address of the executable image.
/// @param time The nanosecond timestamp at which the event occurred.
/// @param index If the function returns true, this value is set to the zero-based index of the image record in the process image list.
/// @return true if an image with the specified load address was alive and located at the specified time, or false if no record exists.
public_function bool
FindImageByBaseAddress
(
    WIN32_PROCESS_INFO *process, 
    uint64_t               addr, 
    uint64_t               time, 
    size_t               &index
)
{
    uint64_t       const      *key_list = &process->ImageBaseAddress[0];
    WIN32_LIFETIME const *lifetime_list = &process->ImageLifetime[0];
    return FindObjectByU64AndTime(key_list, lifetime_list, process->ImageCount, addr, time, index);
}

/// @summary Find an executable image record for the image with the specified attributes. Create a new record if no existing record is found. 
/// @param process The process record to search.
/// @param path The zero-terminated path of the executable image.
/// @param addr The base load address of the executable image in the process address space.
/// @param time The nanosecond timestamp at which the event occurred.
/// @return The zero-based index of the image record within the process image list.
public_function size_t
FindOrCreateImage
(
    WIN32_PROCESS_INFO *process, 
    WCHAR                 *path,
    uint64_t               addr, 
    uint64_t               time
)
{
    size_t   image_index;
    uint32_t const  image_hash = HashWideString(path);
    if (FindImageByName(process, image_hash, time, image_index))
    {   // return the index of the existing record.
        return image_index;
    }
    else
    {   // insert a new, empty record in the process image list.
        WIN32_LIFETIME     lifetime;
        WIN32_IMAGE_INFO image_info;
        image_info.ImagePath = path;
        InitObjectLifetime(lifetime, time);
        image_index = process->ImageCount++;
        process->ImageBaseAddress.push_back(addr);
        process->ImagePathHash.push_back(image_hash);
        process->ImageLifetime.push_back(lifetime);
        process->ImageInfo.push_back(image_info);
        return image_index;
    }
}

/// @summary Retrieve a pointer-size unsigned integer property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The pointer-size integer value, stored in a 64-bit unsigned integer.
public_function uint64_t
TraceEventGetPointer
(
    EVENT_RECORD           *ev, 
    TRACE_EVENT_INFO *info_buf, 
    size_t               index
)
{
    PROPERTY_DATA_DESCRIPTOR  dd;
    DWORD  pdata[2] = {0, 0}; // high DWORD, low DWORD
    ULONG     psize =  8;     // assume 64-bit these days.
    dd.PropertyName = (ULONGLONG)((uint8_t*) info_buf + info_buf->EventPropertyInfoArray[index].NameOffset);
    dd.ArrayIndex   =  ULONG_MAX;
    dd.Reserved     =  0;
    if (ev->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER)   psize = 4;
    if (ev->EventHeader.Flags & EVENT_HEADER_FLAG_64_BIT_HEADER)   psize = 8;
    TdhGetProperty(ev, 0, NULL, 1, &dd, psize, (PBYTE) pdata);
    return  ((uint64_t(pdata[0]) << 32) | uint64_t(pdata[1]));
}

/// @summary Retrieve a 32-bit unsigned integer property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The integer value.
public_function uint32_t
TraceEventGetUInt32
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
public_function int8_t
TraceEventGetSInt8
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
public_function char*
TraceEventGetAnsiStr
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
public_function WCHAR*
TraceEventGetWideStr
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
public_function inline bool
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
public_function inline bool
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
public_function inline bool
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
public_function inline bool
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
public_function inline bool
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
public_function inline bool
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
public_function inline bool
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
public_function inline bool
IsTaskProfilerTaskTransitionEvent
(
    TRACE_EVENT_INFO const *info_buf
)
{
    return IsEqualGUID(info_buf->EventGuid, TaskStateTransitionEventGuid) != FALSE;
}

/// @summary Decode the information from an event of type Process_TypeGroup1 and create or update a WIN32_PROCESS_INFO instance.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa364095(v=vs.85).aspx.
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return A pointer to the WIN32_PROCESS_INFO record that was updated.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_Process_TypeGroup1_Create
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{
    uint32_t const        process_id = TraceEventGetUInt32(ev, info_buf, 1);
    uint64_t const         timestamp = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t   const        process_ix = FindOrCreateProcess(rtev, process_id, timestamp);
    WIN32_PROCESS_INFO *process_info =&rtev->ProcessList.ProcessInfo[process_ix];
    WCHAR              *process_path = TraceEventGetWideStr(ev, info_buf, 7);
    uint32_t const      process_hash = HashWideString(process_path);
    rtev->ProcessList.ProcessHash[process_ix] = process_hash;
    process_info->Executable = process_path;
    UNREFERENCED_PARAMETER(info_size);
    return process_info;
}

/// @summary Decode the information from an event of type Process_TypeGroup1 and update a WIN32_PROCESS_INFO instance.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa364095(v=vs.85).aspx.
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return A pointer to the WIN32_PROCESS_INFO record that was updated.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_Process_TypeGroup1_Terminate
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   UNREFERENCED_PARAMETER(info_size);
    uint32_t const process_id  = TraceEventGetUInt32(ev, info_buf, 1);
    uint64_t const  timestamp  = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t         process_ix  = 0;
    if (FindProcessByPid(rtev, process_id, timestamp, process_ix))
    {   // update the lifetime of the process record.
        rtev->ProcessList.ProcessLifetime[process_ix].DestroyTime = timestamp;
        return &rtev->ProcessList.ProcessInfo[process_ix];
    }
    else
    {   // the process ID doesn't match any known process.
        return NULL;
    }
}

/// @summary Decode the information from an event of type Image_Load and create or update the image list of a WIN32_PROCESS_INFO instance.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa364070%28v=vs.85%29.aspx
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return The WIN32_PROCESS_INFO associated with the process into which the executable image was loaded.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_ImageLoad_Load
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   UNREFERENCED_PARAMETER(info_size);
    uint32_t const        process_id = TraceEventGetUInt32(ev, info_buf, 2);
    uint64_t const         timestamp = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t   const        process_ix = FindOrCreateProcess(rtev, process_id, timestamp);
    WIN32_PROCESS_INFO *process_info =&rtev->ProcessList.ProcessInfo[process_ix];
    WCHAR                *image_path = TraceEventGetWideStr(ev, info_buf, 11);
    uint64_t const        image_base = TraceEventGetPointer(ev, info_buf, 0);
    size_t   const          image_ix = FindOrCreateImage(process_info, image_path, image_base, timestamp);
    UNREFERENCED_PARAMETER (image_ix);
    return process_info;
}

/// @summary Decode the information from an event of type Image_Load, representing an executable image being unloaded from the process address space.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa364070%28v=vs.85%29.aspx
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return The WIN32_PROCESS_INFO associated with the process from which the executable image was unloaded.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_ImageLoad_Unload
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   UNREFERENCED_PARAMETER(info_size);
    uint32_t const        process_id = TraceEventGetUInt32(ev, info_buf, 2);
    uint64_t const         timestamp = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t   const        process_ix = FindOrCreateProcess(rtev, process_id, timestamp);
    WIN32_PROCESS_INFO *process_info =&rtev->ProcessList.ProcessInfo[process_ix];
    uint64_t const        image_base = TraceEventGetPointer(ev, info_buf, 0);
    size_t                  image_ix = 0;

    if (FindImageByBaseAddress(process_info, image_base, timestamp, image_ix))
    {   // update the lifetime of the image record.
        process_info->ImageLifetime[image_ix].DestroyTime = timestamp;
    }
    return process_info;
}

/// @summary Decode the information from an event of type Thread_V2 specifying the creation of a thread within a process and create or update the thread list of a WIN32_PROCESS_INFO instance.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/dd765166%28v=vs.85%29.aspx
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return The WIN32_PROCESS_INFO associated with the process that created the thread.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_Thread_V2_TypeGroup1_Create
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   UNREFERENCED_PARAMETER(info_size);
    uint32_t const        process_id = TraceEventGetUInt32(ev, info_buf, 0);
    uint64_t const         timestamp = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t   const        process_ix = FindOrCreateProcess(rtev, process_id, timestamp);
    WIN32_PROCESS_INFO *process_info =&rtev->ProcessList.ProcessInfo[process_ix];
    uint32_t const         thread_id = TraceEventGetUInt32 (ev, info_buf, 1);
    uint64_t const        entry_addr = TraceEventGetPointer(ev, info_buf, 7);
    size_t   const         thread_ix = FindOrCreateThread(process_info, thread_id, entry_addr, timestamp);
    UNREFERENCED_PARAMETER(thread_ix);
    return process_info;
}

/// @summary Decode the information from an event of type Thread_V2 specifying the termination of a thread within a process.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/dd765166%28v=vs.85%29.aspx
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return The WIN32_PROCESS_INFO associated with the process that created the thread.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_Thread_V2_TypeGroup1_Terminate
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   UNREFERENCED_PARAMETER(info_size);
    uint32_t const        process_id = TraceEventGetUInt32(ev, info_buf, 0);
    uint64_t const         timestamp = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t   const        process_ix = FindOrCreateProcess(rtev, process_id, timestamp);
    WIN32_PROCESS_INFO *process_info =&rtev->ProcessList.ProcessInfo[process_ix];
    uint32_t const         thread_id = TraceEventGetUInt32 (ev, info_buf, 1);
    size_t                 thread_ix = 0;
    
    if (FindThreadByTid(process_info, thread_id, timestamp, thread_ix))
    {   // update the lifetime of the thread record.
        process_info->ThreadLifetime[thread_ix].DestroyTime = timestamp;
    }
    return process_info;
}

/// @summary Decode the information from an event of type ReadyThread indicating when the scheduler transitions a thread to the ready-to-run state.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/dd765158%28v=vs.85%29.aspx
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return The WIN32_PROCESS_INFO associated with the process that created the thread.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_Thread_ReadyThread
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   UNREFERENCED_PARAMETER(info_size);
    uint32_t const        process_id = ev->EventHeader.ProcessId;
    uint64_t const         timestamp = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t   const        process_ix = FindOrCreateProcess(rtev, process_id, timestamp);
    WIN32_PROCESS_INFO *process_info =&rtev->ProcessList.ProcessInfo[process_ix];
    uint32_t const         thread_id = TraceEventGetUInt32(ev, info_buf, 0);
    size_t                 thread_ix = 0;

    if (FindThreadByTid(process_info, thread_id, timestamp, thread_ix))
    {   // push the ready event onto the back of the list.
        WIN32_THREAD_INFO *thread_info = &process_info->ThreadInfo[thread_ix];
        thread_info->ReadyTimes.push_back(timestamp);
        thread_info->ReadyCount++;
    }
    return process_info;
}

/// @summary Decode the information from an event of type CSwitch representing a context switch.
/// See https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744%28v=vs.85%29.aspx
/// @param rtev The profiler events record to update.
/// @param info_buf Metadata associated with the process event.
/// @param info_size The size of the metadata associated with the process event.
/// @param ev The kernel trace event to process.
/// @return The WIN32_PROCESS_INFO associated with the process that created the thread.
public_function WIN32_PROCESS_INFO*
ConsumeKernel_Thread_CSwitch
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{   UNREFERENCED_PARAMETER(info_size);
    uint32_t const        process_id = ev->EventHeader.ProcessId;
    uint64_t const         timestamp = EventTimeToNanoseconds(ev, rtev->ClockFrequency);
    size_t   const        process_ix = FindOrCreateProcess(rtev, process_id, timestamp);
    WIN32_PROCESS_INFO *process_info =&rtev->ProcessList.ProcessInfo[process_ix];
    uint32_t const     new_thread_id = TraceEventGetUInt32(ev, info_buf, 0);
    uint32_t const     old_thread_id = TraceEventGetUInt32(ev, info_buf, 1);
    size_t                 thread_ix = 0;

    if (new_thread_id != 0 && FindThreadByTid(process_info, new_thread_id, timestamp, thread_ix))
    {   // the thread is being switched in.
        WIN32_THREAD_INFO *thread_info = &process_info->ThreadInfo[thread_ix];
        WIN32_SWITCH_IN_DATA      data;
        data.WaitTime  = TraceEventGetUInt32(ev, info_buf, 10); // NewThreadWaitTime
        data.Processor = TraceEventGetSInt8 (ev, info_buf,  9); // OldThreadWaitIdealProcessor
        data.Priority  = TraceEventGetSInt8 (ev, info_buf,  2); // NewThreadPriority
        thread_info->SwitchInTime.push_back(timestamp);
        thread_info->SwitchInData.push_back(data);
        thread_info->SwitchInCount++;
    }
    if (old_thread_id != 0 && FindThreadByTid(process_info, old_thread_id, timestamp, thread_ix))
    {   // the thread is being switched out.
        WIN32_THREAD_INFO *thread_info = &process_info->ThreadInfo[thread_ix];
        WIN32_SWITCH_OUT_DATA     data;
        data.Processor  = TraceEventGetSInt8(ev, info_buf, 9); // OldThreadWaitIdealProcessor
        data.State      = TraceEventGetSInt8(ev, info_buf, 8); // OldThreadState
        data.WaitReason = TraceEventGetSInt8(ev, info_buf, 6); // OldThreadWaitReason
        data.WaitMode   = TraceEventGetSInt8(ev, info_buf, 7); // OldThreadWaitMode
        data.Priority   = TraceEventGetSInt8(ev, info_buf, 3); // OldThreadPriority
        thread_info->SwitchOutTime.push_back(timestamp);
        thread_info->SwitchOutData.push_back(data);
        thread_info->SwitchOutCount++;
    }
    return process_info;
}

// Other ConsumePROVIDER_EVENT functions here
// ... 

/// @summary Dispatches a process start or stop event for data extraction.
/// @param rtev The runtime event data to update.
/// @param info_buf Metadata associated with the trace event.
/// @param info_size The size of the metadata associated with the trace event, in bytes.
/// @param ev The trace event record.
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
        ConsumeKernel_Process_TypeGroup1_Create(rtev, info_buf, info_size, ev);
    }
    if (info_buf->EventDescriptor.Opcode == 2)
    {   // information about a process that terminated while the trace was running.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa364095(v=vs.85).aspx
        ConsumeKernel_Process_TypeGroup1_Terminate(rtev, info_buf, info_size, ev);
    }
}

/// @summary Dispatches a thread start or stop event for data extraction.
/// @param rtev The runtime event data to update.
/// @param info_buf Metadata associated with the trace event.
/// @param info_size The size of the metadata associated with the trace event, in bytes.
/// @param ev The trace event record.
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
        ConsumeKernel_Thread_ReadyThread(rtev, info_buf, info_size, ev);
    }
    if (info_buf->EventDescriptor.Opcode == 36)
    {   // a context switch event. very high frequency.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744(v=vs.85).aspx
        ConsumeKernel_Thread_CSwitch(rtev, info_buf, info_size, ev);
    }
    if (info_buf->EventDescriptor.Opcode ==  1 || info_buf->EventDescriptor.Opcode == 3)
    {   // information about a thread that was just started, or was running when the trace started.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/dd765166(v=vs.85).aspx
        ConsumeKernel_Thread_V2_TypeGroup1_Create(rtev, info_buf, info_size, ev);
    }
    if (info_buf->EventDescriptor.Opcode == 2)
    {   // information about a thread that terminated while the trace was running.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/dd765166(v=vs.85).aspx
        ConsumeKernel_Thread_V2_TypeGroup1_Terminate(rtev, info_buf, info_size, ev);
    }
}

/// @summary Dispatches an image (executable or DLL) load event for data extraction.
/// @param rtev The runtime event data to update.
/// @param info_buf Metadata associated with the trace event.
/// @param info_size The size of the metadata associated with the trace event, in bytes.
/// @param ev The trace event record.
internal_function void
FilterKernelImageLoadEvent
(
    WIN32_PROFILER_EVENTS      *rtev, 
    TRACE_EVENT_INFO       *info_buf, 
    ULONG                  info_size, 
    EVENT_RECORD                 *ev
)
{
    if (info_buf->EventDescriptor.Opcode == 3 || info_buf->EventDescriptor.Opcode == 10)
    {   // information about an image that was just loaded, or was loaded before the trace started. 
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa364070%28v=vs.85%29.aspx
        ConsumeKernel_ImageLoad_Load(rtev, info_buf, info_size, ev);
    }
    if (info_buf->EventDescriptor.Opcode == 2)
    {   // information about an image that was just unloaded.
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa364070%28v=vs.85%29.aspx
        ConsumeKernel_ImageLoad_Unload(rtev, info_buf, info_size, ev);
    }
}

/// @summary Dispatches a trace event from the NT kernel logger provider for data extraction.
/// @param rtev The runtime event data to update.
/// @param info_buf Metadata associated with the trace event.
/// @param info_size The size of the metadata associated with the trace event, in bytes.
/// @param ev The trace event record.
internal_function void
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

/// @summary Dispatches a trace event from the task profiler provider for data extraction.
/// @param rtev The runtime event data to update.
/// @param info_buf Metadata associated with the trace event.
/// @param info_size The size of the metadata associated with the trace event, in bytes.
/// @param ev The trace event record.
internal_function void
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
    UNREFERENCED_PARAMETER(rtev);
    UNREFERENCED_PARAMETER(info_buf);
    UNREFERENCED_PARAMETER(info_size);
    UNREFERENCED_PARAMETER(ev);
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
/// @param trace_file A NULL-terminated string specifying the path of the file to load.
/// @return The profiler events container, or NULL.
public_function WIN32_PROFILER_EVENTS*
NewProfilerEvents
(
    TCHAR const           *trace_file
)
{   // allocate using new to ensure that std::vector constructors run.
    WIN32_PROFILER_EVENTS   *ev = new WIN32_PROFILER_EVENTS();
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
        delete ev;
        return NULL;
    }

    // start a background thread to receive events read from the trace file.
    if ((thread = (HANDLE)_beginthreadex(NULL, 0, EventConsumerThreadMain, ev, 0, &tid)) == NULL)
    {   // without the background thread, the profiler can't receive the events.
        ConsoleError("ERROR (%S): Unable to start event consumer thread (errno = %d).\n", __FUNCTION__, errno);
        CloseHandle(ev->ConsumerLaunch); ev->ConsumerLaunch = NULL;
        CloseTrace(trace);
        delete ev;
        return NULL;
    }

    // TODO(rlk): might want to move to a memory arena system based around VirtualAlloc.
    // this would provide better performance and probably lead to less memory waste.
    ev->EventBuffer              =(uint8_t*) malloc(64 * 1024); // 64KB
    ev->EventBufferSize          = 64 * 1024;
    ev->ConsumerHandle           = trace;
    ev->ConsumerThread           = thread;
    ev->ConsumerThreadId         = tid;
    ev->PointerSize              =(size_t  ) logfile.LogfileHeader.PointerSize;
    ev->TimerResolution          =(uint64_t) logfile.LogfileHeader.TimerResolution;
    ev->ClockFrequency           = logfile.LogfileHeader.PerfFreq;
    ev->ProcessList.ProcessCount = 0;

    // TODO(rlk): initialize other event data containers here.
    // ...

    // start populating our data structures with event data from the trace file.
    SetEvent(ev->ConsumerLaunch);
    return ev;
}

/// @summary Free all resources associated with a profiler event container.
/// @param events The profiler event container to delete, returned by NewProfilerEvents().
public_function void
DeleteProfilerEvents
(
    WIN32_PROFILER_EVENTS **events
)
{
    if (events != NULL)
    {
        WIN32_PROFILER_EVENTS *ev = *events;
        if (ev->ConsumerThread != NULL)
        {   // block the calling thread indefinitely until all events have been consumed.
            // typically, the thread will have already exited by the time this is called.
            WaitForSingleObject(ev->ConsumerThread, INFINITE);
            CloseHandle(ev->ConsumerThread);
            ev->ConsumerThread = NULL;
        }
        if (ev->ConsumerLaunch != NULL)
        {   // close the manual-reset event used to launch the consumer thread. nothing is waiting on it now.
            CloseHandle(ev->ConsumerLaunch);
            ev->ConsumerLaunch = NULL;
        }
        if (ev->ConsumerHandle != INVALID_PROCESSTRACE_HANDLE)
        {   // close the trace handle. typically, this handle will already be closed by the time this is called.
            CloseTrace(ev->ConsumerHandle);
            ev->ConsumerHandle = INVALID_PROCESSTRACE_HANDLE;
        }
        if (ev->EventBuffer != NULL)
        {   // free the memory allocated for event parsing.
            free(ev->EventBuffer);
            ev->EventBuffer = NULL;
            ev->EventBufferSize = 0;
        }
        delete ev; *events = NULL;
    }
}

