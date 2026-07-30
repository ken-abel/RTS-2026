###FINAL REFLECTION

##Project: 
A flight control system that handles priority inversion, built to demonstrate the importance of proper mutex implementation in an aerospace embedded system.

##What I would do differently:
If I could have taken a different approach, I think I would have kept the medium-priority task on a relatively fixed interval, and increased the amount of running tasks. Maybe a bunch of small medium-priority tasks, each outputting various things to the log and simulating various tasks. This would have made the system more realistic, the timing analysis more specific, and the response time of the high-priority task more variable.

However, I think that the manual-activation approach was a good one. You could see, on demand, relatively consistently, a priority inversion at the click of a button. It took exaggerated computation times and a significantly slower control loop period, but the demonstration was obvious, even if the system wasn't EDF feasible while the medium task ran.

##What was harder than expected:
I found the integration of multiple FreeRTOS skills to be one of the harder parts of this project. I wanted to focus on demonstrating priority inversion, but also wanted to implement other things I learned in this class into the project without stealing the spotlight from the main event. That's a large reason that I added the small fourth task- the software timer using semaphore signaling. It gave me practice implementing semaphores and enhanced my understanding of them, even though FreeRTOS has a timer primitive, and the ESP32 probably has a hardware timer that's capable of triggering interrupts. 

Similarly to implementing the software timer task, repurposing the button in App 4's skeleton to be a trigger for unblocking the Terrain Mapping task was a challenge in understanding FreeRTOS and using semaphores for signaling. I feel capable of reading code, understanding it, and altering it for my own purposes, including FreeRTOS primitives which can be intimidating the first time you try to decipher them. Despite this success, I found it hard to balance adding more features without diluting the core emphasis of my presentation- priority inversion prevented using priority inheritance, a feature specific to mutexes.

##The most valuable thing I learned:
The most valuable thing I learned in the class, and emphasized during this project, is the importance of task priorities and execution time. Resources are limited. I have experience using FreeRTOS outside of this class, but used sufficiently powerful chips that neither utilization nor deadlines were factors I had to consider. I had heard of race conditions and priority inversion, but I wasn't sure how they occurred or how to prevent them. All my tasks had the same priority, and used round-robin scheduling. I was mindful to write efficient code, but that was the extent of my knowledge and efforts in utilizing resources effectively. After learning more skills in this class, this project gave me yet another valuable opportunity to experience the nuances of task management in real-time operating systems. I was able to not only identify a scenario where priority inversion could occur, I could manufacture such a scenario and reproduce it predictably, by tuning task priority and execution times.
