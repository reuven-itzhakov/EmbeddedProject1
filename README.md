# RUN
please run the next prompt in the terminal to start the program:
./run.sh [interval]

# INFO
note: the [interval] is the frequency of the data to be updated and it can be any number such as int or float.

# ABOUT
a little info about the program:
every sensor reads the data from the given /proc/ file, and prints it to the given /tmp/ file.
every sensor gets an [interval] as an input (you can see that we are using the parameter in ./run.sh as $1)
after terminating using Ctrl+C the sysmond program is terminated but the sensor readers doesn't. (just close the terminal)