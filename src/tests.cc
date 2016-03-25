#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define UNUSED(x)            (x)
#define public_function      static
#define internal_function    static

public_function intptr_t
Rmost
(
    uint64_t const *a, 
    intptr_t        l, 
    intptr_t        r, 
    uint64_t        v
)
{   // used to find index_upper, v is range_upper. a[l..r).
    while (r - l > 1)
    {
        intptr_t m = l + (r - l) / 2;
        if (a[m]  <= v)   l = m;
        else r = m;
    }
    return l;
}

public_function intptr_t
Lmost
(
    uint64_t const *a, 
    intptr_t        l, 
    intptr_t        r, 
    uint64_t        v
)
{   // used to find index_lower, v is range_lower. a(l..r].
    while (r - l > 1)
    {
        intptr_t m = l + (r - l) / 2;
        if (a[m]  >= v) r = m;
        else l = m;
    }
    return r;
}

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

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    uint64_t sample_times[20] = 
    { 
         1,  1,  1,  5,  5, 
         5,  8,  8, 10, 11, 
        12, 13, 13, 14, 15, 
        17, 17, 17, 20, 20 
    };
    size_t index_lower  = 0;
    size_t index_upper  = 0;
    size_t output_count = 0;

    // 17, 17 returns a valid result.
    // 20, 20 returns an invalid result!
    // 20, 30 returns an invalid result!
    if (FindEventsInTimeRange(sample_times, 20, 9, 16, index_lower, index_upper, output_count))
    {
        printf("Found %Iu items from [%Iu...%Iu].\n", output_count, index_lower, index_upper);
    }

    return 0;
}

