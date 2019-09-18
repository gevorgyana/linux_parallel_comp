/*
    race condition 
    we have "stolen" the x value from a child
    if has not yet read it, but we already expect to read 
    from the fifo -> by the time the child opens its read end,
    it does not see anything and hangs
   */

/*
	order of reads writes matters - I changed the order and it
	does not hang now
*/