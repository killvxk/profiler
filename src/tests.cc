#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define UNUSED(x)            (x)
#define public_function      static
#define internal_function    static

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
    if (range_upper <= event_times[0])
    {   // early out - the time range occurs prior to the start of the event set.
        output_count = 0;
        return false;
    }
    if (range_lower >= event_times[event_count-1])
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
        intptr_t min_i =  0;
        intptr_t max_i = (intptr_t) (event_count - 1);
        intptr_t mid_i =  max_i / 2;
        do
        {
            if (event_times[mid_i] >= range_lower)
            {   // move the upper bound search index closer to the start of the search space.
                max_i = mid_i - 1;
            }
            else
            {   // move the lower bound search index closer to the end of the search space.
                min_i = mid_i + 1;
            }
            // divide the search space in half for the next iteration.
            mid_i = min_i + ((max_i - min_i) / 2);
        }
        while (min_i <= max_i); /* result => */ index_lower = (size_t)(max_i + 1);
    }
    if (range_upper >= event_times[event_count-1])
    {   // early out for upper bound, which occurs after the end of the event set.
        index_upper  = event_count - 1;
    }
    else
    {   // perform a binary search for the first value greater than the upper bound, 
        // where the previous value is less than or equal to the upper bound.
        intptr_t min_i =  0;
        intptr_t max_i = (intptr_t) (event_count - 1);
        intptr_t mid_i =  max_i / 2;
        do
        {
            if (event_times[mid_i] > range_upper)
            {   // move the upper bound search index closer to the start of the search space.
                max_i = mid_i - 1;
            }
            else
            {   // move the lower bound search index closer to the end of the search space.
                min_i = mid_i + 1;
            }
            // divide the search space in half for the next iteration.
            mid_i = min_i + ((max_i - min_i) / 2);
        }
        while (min_i <= max_i); /* result => */ index_upper = (size_t)(min_i - 1);
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
    if (FindEventsInTimeRange(sample_times, 20, 17, 17, index_lower, index_upper, output_count))
    {
        printf("Found %Iu items from [%Iu...%Iu].\n", output_count, index_lower, index_upper);
    }

    return 0;
}

