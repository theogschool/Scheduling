# Scheduling
Creates a simulation of round robin, shortest time and mlfq schedulers in c using a linked list and threads. 

The tasks file is of the format: task_type, task priority, amount of time a task will take, odds a task will simulate an i/o operation (wait)

# Running my program:

1. Run the make command.
2. Run the command "./implement x y" where x is the amount of cpus and y is the policy.

My program uses timing as described in the assignment document.  

Insure a file exist named "tasks.txt" in the same directory as the make file and has all the tasks that you wish to be performed on it.  

Run "make clean" to clear previously made executables.

By Theo Gerwing
