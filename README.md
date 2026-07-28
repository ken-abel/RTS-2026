A flight control system that handles priority inversion, built to demonstrate the importance of proper mutex implementation in an
aerospace embedded system.

A small drone, deployed from a nearby spacecraft currently orbiting Mars, needs to adjust its attitude and trajectory, map the surface, 
and routinely log its flight path to a database located on the larger spacecraft.

| Task             | Priority | Function                                                     |
| ---------------- | -------- | ------------------------------------------------------------ |
| Flight Control   | High     | Computes engine thrust and attitude corrections every 10 ms. | Note: this period has been slowed to 1s for the sake of demonstrating priority inversion
| Terrain Mapping  | Medium   | Processes camera and lidar data to avoid hazards.            |
| Telemetry Logger | Low      | Uploads flight data to nearby host ship.                     |

The Telemetry Logger locks the navigation mutex to upload current positional data to the nearby spacecraft.
The Flight Control task periodically becomes ready each control cycle, requiring the navigation mutex to operate.
The Terrain Mapping task processes lidar and camera frames upon manual request from the nearby spacecraft (simulated by pressing the button). It
is very time and resource heavy.

The Flight Control task needs to be able to quickly respond to disturbances or detected obstacles and hazards. Due to the presence
of the mutex and the given priority hierarchy, priority inversion is possible. So, it is important to enable priority inheritance so the Terrain 
Mapping task doesn't preempt the Telemetry Logger while the Flight Control task is urgently waiting for the mutex.
To simulate having priority inheritance disabled, a binary semaphore can be used in place of a mutex by setting USE_PI_MUTEX to 0.


Capabilities from each application:
App 4 is the skeleton. Specifically, the priority inversion demo.
Folded in the LED and vTaskDelay functionality from App 1,
as well as the binary semaphore signaling from App 3 to enable signaling
H and M tasks to start (H via vTaskDelayUntil loop from App 1, M via GPIO
interrupts and binary semaphore signaling).

To-do
Edit initial comment block to reflect that this is the final app and not App4
Delete irrelevant tasks and lines of code from the skeleton

Things I noticed:
With the lock set to be a mutex, if you spam the button, the M task will starve the L task and actually significantly decrease the latency
of the H task (from 220ms at times, to consistently ~2ms).
With the lock set to be a semaphore, if you press the button, it consistently causes H's delay to be greater than 1 seconds, sometimes 3 seconds or more
compared to never breaching 0.3 seconds when using a mutex with priority inheritance.
The period of M is variable, so when I calculate total utilization U, I should find the threshold of M's period at which U = .78 and 1
App2 has a WCET measurer that I can use to find the execution time of L M and H