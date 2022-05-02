# Overriding-DoPeek-for-CoDel-and-FqCoDel-in-ns-3

Clone the repository of ns3 - `git clone https://gitlab.com/nsnam/ns-3-dev`

Description of the issue: https://gitlab.com/nsnam/ns-3-dev/-/issues/209

<-------------------------------------------------------------------------------------------------------------------------------------------------------->

Peek() and DoPeek() function in ns-3 is used to get a copy of next element to be dequed from the queue. However, the implementation of Peek() and DoPeek() is such that Peek() calls DoPeek() function and first an Item is dequeued and stored in a pointer variable called m_requeued. Even if the Item is dequeued it is still considered as a part of the queue. This behaviour of the Peek and DoPeek is motivated from the Linux Kernal implementation of the function qdisc_peek_dequeued. This is recommended for the queue discs in which the next packet to be extracted is not obvious.  

Default `DoPeek()` function in NS-3:
 https://gitlab.com/nsnam/ns-3-dev/-/blob/master/src/traffic-control/model/queue-disc.cc#L935
 
 Complete view of router queue given below, when a peek is called- 
 
 Router queue = Output port queue + m_requeued
 
 ```
 --------------------------------------------------------------------
 |  --------------------------------------------      ------------  |
 |           |            |            |              |          |  |   
 |           |            |            |              |          |  |
 |  --------------------------------------------      ------------  |
 |              Output port queue                      m_requeued   |
 --------------------------------------------------------------------
 ```

**CoDel** 

Codel is an AQM(Active Queue Management) algorithm, which works at the output port, in dequeue time. It calculates `current_queue_delay` during dequeue time and based on its value, decides whether to drop or mark this packet / Item.

To read more about CoDel: https://datatracker.ietf.org/doc/html/rfc8289

Existing `Peek()` and `DoPeek()` have some issues when it comes to the implementation of codel. As `Peek()` in codel, inherits the `Peek()` and `DoPeek()` of `queue-disc.cc`. So, on calling of peek function it first dequeue the packet and perfom all the marking needed at the time of dequeue and then store it in `m_requeued` variable. But, the packet is still inside the queue. Hence, when we call `Dequeue()`, then it will first check m_requeued variable and if it is not empty then we will dequeue that `m_requeued` varible first without doing any marking. This is a problem with the codel because codel works at dequeue time. We are doing all the computation on calling `Peek()` which needed to be done on calling `Dequeue()`.

**Class diagram:**

```
-------------------------
|     CodelQueueDisc    |
|-----------------------|  
|      DoDequeue()      |
|      DoEnqueue()      |
-------------------------
         |
         |
         | Inherits
         '--------------> -------------------------
                          |       QueueDisc       |
                          |-----------------------|  
                          |        Peek()         |
                          |       DoPeek()        |
                          -------------------------
```

`CodelQueueDisc` class don't implement it's own `Peek()` & `DoPeek()`. It uses the functions defined in its base class `QueueDics`.Hence, to overcome this problem, it is required to override the `Peek()` & `"Dopeek()` function in `CodelQueueDisc` class, which doesn't perform a real internal dequeue.
 
