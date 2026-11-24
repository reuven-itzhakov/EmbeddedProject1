# RUN
please run the next prompt in the terminal to start the program:
./bin/sysmond <sysmon_interval> <mem_interval> <cpu_interval> <proc_interval>
where the intervals is integers for sleep()

# INFO
note: the <interval> is the frequency of the data to be updated and it can be any number such as int or float.

# ABOUT
i learn just now about the poll() and i had to use it.
also i refactored the code so the main will have only calls for functions
thats do the main code - like instead of creating the fifos and opening them in the main
i am doing it in functions.