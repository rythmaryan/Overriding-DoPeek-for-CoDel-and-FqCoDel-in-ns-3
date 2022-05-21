#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-size.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("A NAME");

// The following code borrowed from Linux codel.h, for unit testing
#define REC_INV_SQRT_BITS_ns3 (8 * sizeof(uint16_t))
/* or sizeof_in_bits(rec_inv_sqrt) */
/* needed shift to get a Q0.32 number from rec_inv_sqrt */
#define REC_INV_SQRT_SHIFT_ns3 (32 - REC_INV_SQRT_BITS_ns3)

static uint16_t _codel_Newton_step (uint16_t rec_inv_sqrt, uint32_t count)
{
  uint32_t invsqrt = ((uint32_t)rec_inv_sqrt) << REC_INV_SQRT_SHIFT_ns3;
  uint32_t invsqrt2 = ((uint64_t)invsqrt * invsqrt) >> 32;
  uint64_t val = (3LL << 32) - ((uint64_t)count * invsqrt2);

  val >>= 2; /* avoid overflow in following multiply */
  val = (val * invsqrt) >> (32 - 2 + 1);
  return static_cast<uint16_t>(val >> REC_INV_SQRT_SHIFT_ns3);
}

static uint32_t _reciprocal_scale (uint32_t val, uint32_t ep_ro)
{
  return (uint32_t)(((uint64_t)val * ep_ro) >> 32);
}
// End Linux borrow

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Codel Queue Disc Test Item
 */
class CodelQueueDiscTestItem : public QueueDiscItem {
public:
  /**
   * Constructor
   *
   * \param p packet
   * \param addr address
   * \param ecnCapable ECN capable
   */
  CodelQueueDiscTestItem (Ptr<Packet> p, const Address & addr, bool ecnCapable);
  virtual ~CodelQueueDiscTestItem ();

  // Delete copy constructor and assignment operator to avoid misuse
  CodelQueueDiscTestItem (const CodelQueueDiscTestItem &) = delete;
  CodelQueueDiscTestItem & operator = (const CodelQueueDiscTestItem &) = delete;

  virtual void AddHeader (void);
  virtual bool Mark(void);

private:
  CodelQueueDiscTestItem ();
  
  bool m_ecnCapablePacket; ///< ECN capable packet?
};

CodelQueueDiscTestItem::CodelQueueDiscTestItem (Ptr<Packet> p, const Address & addr, bool ecnCapable)
  : QueueDiscItem (p, addr, ecnCapable),
    m_ecnCapablePacket (ecnCapable)
{
}

CodelQueueDiscTestItem::~CodelQueueDiscTestItem ()
{
}

void
CodelQueueDiscTestItem::AddHeader (void)
{
}

bool
CodelQueueDiscTestItem::Mark (void)
{
  if (m_ecnCapablePacket)
    {
      return true;
    }
  return false;
}


/*
  Test Peek functionality in CoDel without ECN  
*/
class CoDelPeekTest : public TestCase{
  public:
    CoDelPeekTest(QueueSizeUnit mode);
    void DoRun(void);
  private:
    void Enqueue(Ptr<CoDelQueueDisc> queue, uint32_t pktSize, uint32_t nPkt);
    void Dequeue(Ptr<CoDelQueueDisc> queue);
    void Peek(Ptr<CoDelQueueDisc> queue);
    QueueSizeUnit m_mode;
    uint32_t m_peeked;
};

CoDelPeekTest::CoDelPeekTest(QueueSizeUnit mode):
  TestCase("Basic Codel Peek test"){
    m_mode = mode;
}

void 
CoDelPeekTest::DoRun(){
  Ptr<CoDelQueueDisc> queue = CreateObject<CoDelQueueDisc> ();
  uint32_t pktSize = 1000;
  uint32_t modeSize = 0;
  if (m_mode == QueueSizeUnit::BYTES){
      modeSize = pktSize;
  }
  else if (m_mode == QueueSizeUnit::PACKETS){
      modeSize = 1;
  }
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("MaxSize", QueueSizeValue (QueueSize (m_mode, modeSize * 500))),
                         true, "Verify that we can actually set the attribute MaxSize");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("PeekFunction", BooleanValue (true)),
                         true, "Verify that we can actually set the attribute PeekFunction");
  queue->Initialize ();

  Enqueue(queue, pktSize, 20);
  NS_LOG_UNCOND("Enqueue Done");
  Time firstDequeueTime = 0.3 * queue->GetTarget();
  Simulator::Schedule(firstDequeueTime, &CoDelPeekTest::Peek, this, queue);
  Time nextDequeueTime =  0.5 * queue->GetTarget();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  nextDequeueTime =  2 * queue->GetTarget();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  nextDequeueTime =  2.5 * queue->GetTarget();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime + queue->GetInterval();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime *1.5;
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime + queue->GetInterval();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime + queue->GetInterval();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime *1.5;
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Peek, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTest::Dequeue, this, queue);
  Simulator::Run();
  Simulator::Destroy();
}

void 
CoDelPeekTest::Enqueue(Ptr<CoDelQueueDisc> queue, uint32_t pktSize, uint32_t nPkt){
  Address addr;
  for (uint32_t i=0; i<nPkt; i++){
    Ptr<Packet> p = Create<Packet>(pktSize);
    queue->Enqueue(Create<CodelQueueDiscTestItem>(p, addr, false));
  }
}

void
CoDelPeekTest::Dequeue(Ptr<CoDelQueueDisc> queue){
  uint64_t time = Simulator::Now().GetMilliSeconds();
  uint32_t beforeSize = queue->GetCurrentSize().GetValue();
  uint32_t beforeDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  Ptr<QueueDiscItem> item = queue->Dequeue();
  m_peeked = false;
  if(item==0)
    NS_LOG_UNCOND("Queue Empty");
  uint32_t afterSize = queue->GetCurrentSize().GetValue();
  uint32_t afterDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  NS_LOG_UNCOND("At " << time << "ms Dequeue");
  NS_LOG_UNCOND("Packet id Dequeued " << item->GetPacket()->GetUid());
  NS_LOG_UNCOND("Queue Size before - " << beforeSize << " | Packets Dropped Before - " << beforeDroppedDequeue);
  NS_LOG_UNCOND("Queue Size after  - " << afterSize << " | Packets Dropped After  - " << afterDroppedDequeue << std::endl);
}

void
CoDelPeekTest::Peek(Ptr<CoDelQueueDisc> queue){
  uint64_t time = Simulator::Now().GetMilliSeconds();
  uint32_t beforeSize = queue->GetCurrentSize().GetValue();
  uint32_t beforeDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  Ptr<const QueueDiscItem> item = queue->Peek();
  m_peeked = true;
  if(item==0)
    NS_LOG_UNCOND("Queue Empty");
  uint32_t afterSize = queue->GetCurrentSize().GetValue();
  uint32_t afterDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  NS_LOG_UNCOND("At " << time << "ms Peek");
  NS_LOG_UNCOND("Packet id Peeked " << item->GetPacket()->GetUid());
  NS_LOG_UNCOND("Queue Size before - " << beforeSize << " | Packets Dropped Before - " << beforeDroppedDequeue);
  NS_LOG_UNCOND("Queue Size after  - " << afterSize << " | Packets Dropped After  - " << afterDroppedDequeue << std::endl);
}



/*
  Test Peek functionality in CoDel with ECN
*/
class CoDelPeekTestMark : public TestCase{
  public:
    CoDelPeekTestMark(QueueSizeUnit mode);
    void DoRun(void);
  private:
    void Enqueue(Ptr<CoDelQueueDisc> queue, uint32_t pktSize, uint32_t nPkt);
    void Dequeue(Ptr<CoDelQueueDisc> queue);
    void Peek(Ptr<CoDelQueueDisc> queue);
    QueueSizeUnit m_mode;
    uint32_t m_peeked;
};

CoDelPeekTestMark::CoDelPeekTestMark(QueueSizeUnit mode):
  TestCase("Basic Codel Peek test"){
    m_mode = mode;
}

void 
CoDelPeekTestMark::DoRun(){
  Ptr<CoDelQueueDisc> queue = CreateObject<CoDelQueueDisc> ();
  uint32_t pktSize = 1000;
  uint32_t modeSize = 0;
  if (m_mode == QueueSizeUnit::BYTES){
      modeSize = pktSize;
  }
  else if (m_mode == QueueSizeUnit::PACKETS){
      modeSize = 1;
  }
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("MaxSize", QueueSizeValue (QueueSize (m_mode, modeSize * 500))),
                         true, "Verify that we can actually set the attribute MaxSize");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("UseEcn", BooleanValue (true)),
                         true, "Verify that we can actually set the attribute UseEcn");
  NS_TEST_EXPECT_MSG_EQ (queue->SetAttributeFailSafe ("PeekFunction", BooleanValue (true)),
                         true, "Verify that we can actually set the attribute PeekFunction");
  queue->Initialize ();

  Enqueue(queue, pktSize, 20);
  NS_LOG_UNCOND("Enqueue Done");
  Time firstDequeueTime = 0.3 * queue->GetTarget();
  Simulator::Schedule(firstDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  Time nextDequeueTime =  0.5 * queue->GetTarget();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  nextDequeueTime =  2 * queue->GetTarget();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  nextDequeueTime =  2.5 * queue->GetTarget();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime + queue->GetInterval();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime *1.5;
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime + queue->GetInterval();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime + queue->GetInterval();
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Dequeue, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  nextDequeueTime =  nextDequeueTime *1.5;
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Peek, this, queue);
  Simulator::Schedule(nextDequeueTime, &CoDelPeekTestMark::Dequeue, this, queue);
  Simulator::Run();
  Simulator::Destroy();
  
 

  
}

void 
CoDelPeekTestMark::Enqueue(Ptr<CoDelQueueDisc> queue, uint32_t pktSize, uint32_t nPkt){
  Address addr;
  for (uint32_t i=0; i<nPkt; i++){
    Ptr<Packet> p = Create<Packet>(pktSize);
    queue->Enqueue(Create<CodelQueueDiscTestItem>(p, addr, true));
  }
}

void
CoDelPeekTestMark::Dequeue(Ptr<CoDelQueueDisc> queue){
  uint64_t time = Simulator::Now().GetMilliSeconds();
  uint32_t beforeSize = queue->GetCurrentSize().GetValue();
  uint32_t beforeDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  uint32_t beforeMarked = queue->GetStats().GetNMarkedPackets(CoDelQueueDisc::TARGET_EXCEEDED_MARK);
  Ptr<QueueDiscItem> item = queue->Dequeue();
  m_peeked = false;
  if(item==0)
    NS_LOG_UNCOND("Queue Empty");
  uint32_t afterSize = queue->GetCurrentSize().GetValue();
  uint32_t afterDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  uint32_t afterMarked = queue->GetStats().GetNMarkedPackets(CoDelQueueDisc::TARGET_EXCEEDED_MARK);
  NS_LOG_UNCOND("At " << time << "ms Dequeue");
  NS_LOG_UNCOND("Packet id Dequeued " << item->GetPacket()->GetUid());
  NS_LOG_UNCOND("Queue Size before - " << beforeSize << " | Packets Dropped before - " << beforeDroppedDequeue << " | Packets marked before - " << beforeMarked);
  NS_LOG_UNCOND("Queue Size after  - " << afterSize << " | Packets Dropped after  - " << afterDroppedDequeue << " | Packets marked after  - " << afterMarked << std::endl);
}

void
CoDelPeekTestMark::Peek(Ptr<CoDelQueueDisc> queue){
  uint64_t time = Simulator::Now().GetMilliSeconds();
  uint32_t beforeSize = queue->GetCurrentSize().GetValue();
  uint32_t beforeDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  uint32_t beforeMarked = queue->GetStats().GetNMarkedPackets(CoDelQueueDisc::TARGET_EXCEEDED_MARK);
  Ptr<const QueueDiscItem> item = queue->Peek();
  m_peeked = true;
  if(item==0)
    NS_LOG_UNCOND("Queue Empty");
  uint32_t afterSize = queue->GetCurrentSize().GetValue();
  uint32_t afterDroppedDequeue = queue->GetStats().GetNDroppedPackets(CoDelQueueDisc::TARGET_EXCEEDED_DROP);
  uint32_t afterMarked = queue->GetStats().GetNMarkedPackets(CoDelQueueDisc::TARGET_EXCEEDED_MARK);
  NS_LOG_UNCOND("At " << time << "ms Peek");
  NS_LOG_UNCOND("Packet id Peeked " << item->GetPacket()->GetUid());
  NS_LOG_UNCOND("Queue Size before - " << beforeSize << " | Packets Dropped before - " << beforeDroppedDequeue << " | Packets marked before - " << beforeMarked);
  NS_LOG_UNCOND("Queue Size after - " << afterSize << " | Packets Dropped after - " << afterDroppedDequeue << " | Packets marked after - " << afterMarked << std::endl);
}


int main(){
    NS_LOG_UNCOND("------------------------------");
    NS_LOG_UNCOND("Test 5 for CoDel without Marking");
    NS_LOG_UNCOND("------------------------------");
    CoDelPeekTest gp(QueueSizeUnit::PACKETS);
    gp.DoRun();


    NS_LOG_UNCOND("-------------------------------------");
    NS_LOG_UNCOND("Test 5 for CoDel with Marking(only ECN)");
    NS_LOG_UNCOND("-------------------------------------");
    CoDelPeekTestMark gm(QueueSizeUnit::PACKETS);
    gm.DoRun();
    return 0;
}