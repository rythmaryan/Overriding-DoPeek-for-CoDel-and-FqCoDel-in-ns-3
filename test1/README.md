The test was done on codel qdisc with 2 peek functions under consideration

1. The default peek already available in ns3 queus-disc.cc which dequeues a packet during peek time and stores it in m_requeued, and when dequeue is called it just sends it without further checking.
2. The peek function implemented in Cobalt-queue-dics, which just returns the head item on the queue.

Experiment time line -

      2 * target                       2 * interval
0ms             10ms                                                 210ms                               
----------------------------------------------------------------------------------------------------------
enqueue     1st dequeue and peek              timer                2nd dequeue
all            timer                         expires                 
packets        starts                       at 110 ms

Our aim of the this 1st experiment is to check the behaviour of peek, when peek is called before the timer expires and subsequently dequeue is called after the timer expires.

Results -

1. peek_dequeued - It peeks the correct packet as during the peek time, packet should not be dropped. However, during the subsequent dequeue time, packet is not dropped even if the timer has expired.

2. peek_head - This function behaves as expected, it peeks the correct packet and also drops the packet during the dequeue time(as timer expired). However the packet peeked and dequeued is different.