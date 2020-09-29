/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 Stanford University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Serhat Arslan <sarslan@stanford.edu>
 *
 * Network topology
 *
 *          2Gb/s, 1ms         1Gb/s, 1ms         2Gb/s, 1ms
 *    n2-----------------n0-----------------n1-----------------n(numflows+2)
 *    n3----------------/                    \-----------------n(numflows+3)
 *    ...                                                               ...
 *    n(numflows+1)----/                      \----------------n(2numflows+2)
 *
 *  Usage (e.g.): ./waf --run 'simple-tcp-stats --num_flows=...'
 */

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
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleTcpStats");

static bool firstCwnd = true;
static bool firstSshThr = true;
static bool firstRtt = true;
// static bool firstRto = true;
static Ptr<OutputStreamWrapper> cWndStream;
static Ptr<OutputStreamWrapper> ssThreshStream;
static Ptr<OutputStreamWrapper> rttStream;
static Ptr<OutputStreamWrapper> rtoStream;
static Ptr<OutputStreamWrapper> nextTxStream;
static Ptr<OutputStreamWrapper> nextRxStream;
static Ptr<OutputStreamWrapper> inFlightStream;
static uint32_t cWndValue;
static uint32_t ssThreshValue;
static double TRACE_START_TIME = 1.0;

static void
CwndTracer (uint32_t oldval, uint32_t newval)
{
  if (firstCwnd)
  {
    *cWndStream->GetStream () << TRACE_START_TIME << " " << oldval << std::endl;
    firstCwnd = false;
  }
  *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
  cWndValue = newval;

  if (!firstSshThr)
  {
    *ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << ssThreshValue << std::endl;
  }
}

// static void
// SsThreshTracer (uint32_t oldval, uint32_t newval)
// {
//   if (firstSshThr)
//   {
//     *ssThreshStream->GetStream () << TRACE_START_TIME << " " << oldval << std::endl;
//     firstSshThr = false;
//   }
//   *ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
//   ssThreshValue = newval;
//
//   if (!firstCwnd)
//   {
//     *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << cWndValue << std::endl;
//   }
// }

static void
RttTracer (Time oldval, Time newval)
{
  if (firstRtt)
  {
    *rttStream->GetStream () << TRACE_START_TIME << " " << oldval.GetSeconds () << std::endl;
    firstRtt = false;
  }
  *rttStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

// static void
// RtoTracer (Time oldval, Time newval)
// {
//   if (firstRto)
//   {
//     *rtoStream->GetStream () << TRACE_START_TIME << " " << oldval.GetSeconds () << std::endl;
//     firstRto = false;
//   }
//   *rtoStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
// }
//
// static void
// NextTxTracer (SequenceNumber32 old, SequenceNumber32 nextTx)
// {
//   NS_UNUSED (old);
//   *nextTxStream->GetStream () << Simulator::Now ().GetSeconds () << " " << nextTx << std::endl;
// }
//
// static void
// InFlightTracer (uint32_t old, uint32_t inFlight)
// {
//   NS_UNUSED (old);
//   *inFlightStream->GetStream () << Simulator::Now ().GetSeconds () << " " << inFlight << std::endl;
// }
//
// static void
// NextRxTracer (SequenceNumber32 old, SequenceNumber32 nextRx)
// {
//   NS_UNUSED (old);
//   *nextRxStream->GetStream () << Simulator::Now ().GetSeconds () << " " << nextRx << std::endl;
// }

static void
TraceCwnd (std::string cwnd_tr_file_name)
{
  AsciiTraceHelper ascii;
  cWndStream = ascii.CreateFileStream (cwnd_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));
}

// static void
// TraceSsThresh (std::string ssthresh_tr_file_name)
// {
//   AsciiTraceHelper ascii;
//   ssThreshStream = ascii.CreateFileStream (ssthresh_tr_file_name.c_str ());
//   Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold", MakeCallback (&SsThreshTracer));
// }

static void
TraceRtt (std::string rtt_tr_file_name)
{
  AsciiTraceHelper ascii;
  rttStream = ascii.CreateFileStream (rtt_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback (&RttTracer));
}

// static void
// TraceRto (std::string rto_tr_file_name)
// {
//   AsciiTraceHelper ascii;
//   rtoStream = ascii.CreateFileStream (rto_tr_file_name.c_str ());
//   Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/RTO", MakeCallback (&RtoTracer));
// }
//
// static void
// TraceNextTx (std::string &next_tx_seq_file_name)
// {
//   AsciiTraceHelper ascii;
//   nextTxStream = ascii.CreateFileStream (next_tx_seq_file_name.c_str ());
//   Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/NextTxSequence", MakeCallback (&NextTxTracer));
// }
//
// static void
// TraceInFlight (std::string &in_flight_file_name)
// {
//   AsciiTraceHelper ascii;
//   inFlightStream = ascii.CreateFileStream (in_flight_file_name.c_str ());
//   Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight", MakeCallback (&InFlightTracer));
// }
//
// static void
// TraceNextRx (std::string &next_rx_seq_file_name)
// {
//   AsciiTraceHelper ascii;
//   nextRxStream = ascii.CreateFileStream (next_rx_seq_file_name.c_str ());
//   Config::ConnectWithoutContext ("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/RxBuffer/NextRxSequence", MakeCallback (&NextRxTracer));
// }

int main (int argc, char *argv[])
{
  std::string transport_prot = "TcpNewReno";
  double error_p = 0.0;
  std::string bandwidth = "50Mbps";
  std::string access_bandwidth = "100Mbps";
  std::string link_delay = "1ms";
  bool tracing = false;
  std::string prefix_file_name = "SimpleTcpStats";
  uint64_t data_mbytes = 100e9;
  uint32_t mtu_bytes = 1500;
  uint16_t num_flows = 1;
  double duration = 100.0;
  uint32_t run = 0;
  bool flow_monitor = false;
  bool pcap = false;
  bool sack = false;
  std::string queue_type = "ns3::DropTailQueue";
  std::string recovery = "ns3::TcpClassicRecovery";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpNewReno, "
                "TcpLinuxReno, TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, "
                "TcpScalable, TcpVeno, TcpBic, TcpYeah, TcpIllinois, "
                "TcpWestwood, TcpWestwoodPlus, TcpLedbat, TcpLp, TcpDctcp",
                transport_prot);
  cmd.AddValue ("error_p", "Packet error rate", error_p);
  cmd.AddValue ("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue ("access_bandwidth", "Access link bandwidth", access_bandwidth);
  cmd.AddValue ("link_delay", "Link delay", link_delay);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("prefix_name", "Prefix of output trace file", prefix_file_name);
  cmd.AddValue ("data", "Number of Megabytes of data to transmit", data_mbytes);
  cmd.AddValue ("mtu", "Size of IP packets to send in bytes", mtu_bytes);
  cmd.AddValue ("num_flows", "Number of flows", num_flows);
  cmd.AddValue ("duration", "Time to allow flows to run in seconds", duration);
  cmd.AddValue ("run", "Run index (for setting repeatable seeds)", run);
  cmd.AddValue ("flow_monitor", "Enable flow monitor", flow_monitor);
  cmd.AddValue ("pcap_tracing", "Enable or disable PCAP tracing", pcap);
  cmd.AddValue ("queue_type",
                "Queue type for gateway (default ns3::DropTailQueue)",
                queue_type);
  cmd.AddValue ("sack", "Enable or disable SACK option", sack);
  cmd.AddValue ("recovery",
                "Recovery algorithm type to use (e.g., ns3::TcpPrrRecovery",
                recovery);
  cmd.Parse (argc, argv);

  transport_prot = std::string ("ns3::") + transport_prot;
  prefix_file_name = prefix_file_name + "-" + bandwidth + "-" + link_delay;

  SeedManager::SetSeed (1);
  SeedManager::SetRun (run);

  // User may find it convenient to enable logging
  LogComponentEnable("SimpleTcpStats", LOG_LEVEL_ALL);
  //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("PfifoFastQueueDisc", LOG_LEVEL_ALL);

  // Calculate the ADU size
  Header* temp_header = new Ipv4Header ();
  uint32_t ip_header = temp_header->GetSerializedSize ();
//   NS_LOG_LOGIC ("IP Header size is: " << ip_header);
  delete temp_header;
  temp_header = new TcpHeader ();
  uint32_t tcp_header = temp_header->GetSerializedSize ();
//   NS_LOG_LOGIC ("TCP Header size is: " << tcp_header);
  delete temp_header;
  uint32_t tcp_adu_size = mtu_bytes - 20 - (ip_header + tcp_header);
  NS_LOG_LOGIC ("TCP ADU size is: " << tcp_adu_size);

  if (sack)
    NS_LOG_LOGIC("Using SACK (Selective Acknowledgements)");


  // 2 MB of TCP buffer
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));

  Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType",
                      TypeIdValue (TypeId::LookupByName (recovery)));
  // Select TCP variant
  if (transport_prot.compare ("ns3::TcpWestwoodPlus") == 0)
  {
    // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                        TypeIdValue (TcpWestwood::GetTypeId ()));
    // the default protocol type in ns3::TcpWestwood is WESTWOOD
    Config::SetDefault ("ns3::TcpWestwood::ProtocolType",
                        EnumValue (TcpWestwood::WESTWOODPLUS));
  }
  else
  {
    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid),
                         "TypeId " << transport_prot << " not found");
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                        TypeIdValue (TypeId::LookupByName (transport_prot)));
  }

  // Create the topology
  NodeContainer gateways;
  gateways.Create (2);

  NodeContainer sources;
  sources.Create (num_flows);
//   NS_LOG_LOGIC("The ID of the first source node is: " <<
//                sources.Get (0)-> GetId ());

  NodeContainer sinks;
  sinks.Create (num_flows);
//   NS_LOG_LOGIC("The ID of the first sink node is: " <<
//                sinks.Get (0)-> GetId ());

  // Configure the error model
  // Here we use RateErrorModel with packet error rate
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  RateErrorModel error_model;
  error_model.SetRandomVariable (uv);
  error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model.SetRate (error_p);

  DataRate access_b (access_bandwidth);
  DataRate bottle_b (bandwidth);
    
  Time serialization = bottle_b.CalculateBytesTxTime (mtu_bytes);
  Time buffer_drain_time = MicroSeconds(100);
  uint32_t num_packets = buffer_drain_time.GetMicroSeconds () / serialization.GetMicroSeconds ();

  Time access_d (link_delay);
  NS_LOG_LOGIC("Bandwidth is: " << bandwidth); 
//   NS_LOG_LOGIC("Access Bandwidth is " << access_bandwidth);

  // Set the simulation start and stop time
  double stop_time = TRACE_START_TIME + duration;

  // uint32_t size = ((static_cast<double>(std::min (access_b, bottle_b).GetBitRate ()) / 8.0) * rtt)/10;
  // NS_LOG_LOGIC("Buffer size (BDP) is: " << size << " bytes.");

  PointToPointHelper BottleneckLink;
  BottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  BottleneckLink.SetChannelAttribute ("Delay", StringValue (link_delay));
  BottleneckLink.SetDeviceAttribute ("ReceiveErrorModel",
                                     PointerValue (&error_model));
  // BottleneckLink.SetQueue (queue_type, "MaxSize",
  //                          QueueSizeValue (QueueSize (QueueSizeUnit::BYTES,
  //                                                     size-mtu_bytes)));
  BottleneckLink.SetQueue (queue_type, "MaxSize", StringValue (std::to_string(num_packets-1)+"p"));

  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  PointToPointHelper AccessLink;
  AccessLink.SetDeviceAttribute ("DataRate", StringValue (access_bandwidth));
  // AccessLink.SetQueue (queue_type, "MaxSize",
  //                      QueueSizeValue (QueueSize (QueueSizeUnit::BYTES,
  //                                                 size-mtu_bytes)));
  AccessLink.SetQueue (queue_type, "MaxSize", StringValue ("1p"));
    
  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", 
                             "MaxSize", StringValue("1p"));

  Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable> ();

  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  Ipv4InterfaceContainer sink_interfaces;
  devices = BottleneckLink.Install (gateways.Get (0), gateways.Get (1));
  address.NewNetwork ();
  interfaces = address.Assign (devices);
   
  Ptr<TrafficControlLayer> tc= devices.Get (0)->GetNode ()->GetObject<TrafficControlLayer> ();;
  Ptr<QueueDisc> qd = tc->GetRootQueueDiscOnDevice(devices.Get (0));
  if(qd)
  {
    qd->SetMaxSize(QueueSize("1p")); //Queue will be accumulated on a DropTail one created above
  }
  else
  {
    tchPfifo.Install (devices.Get (0));
  }
//   NS_LOG_LOGIC("Configured Queue Disc size.");
    
  for (uint32_t i = 0; i < num_flows; i++)
  {
    double rnd_delay = rng->GetValue(0,100);
    if(i==0)
        rnd_delay = 0;
    Time rnd_link_delay = access_d + MicroSeconds (rnd_delay);
    std::string rnd_link_delay_str = std::to_string(rnd_link_delay.GetMilliSeconds ()) + "ms";
    AccessLink.SetChannelAttribute ("Delay", StringValue (rnd_link_delay_str));
      
    if(i==0)
    {
      Time rtt = rnd_link_delay*4+access_d*2+buffer_drain_time;
      NS_LOG_LOGIC("RTT is: " << (rtt).GetMicroSeconds () << "us");
    }

    devices = AccessLink.Install (sources.Get (i), gateways.Get (0));
    address.NewNetwork ();
    interfaces = address.Assign (devices);

    devices = AccessLink.Install (gateways.Get (1), sinks.Get (i));
    address.NewNetwork ();
    interfaces = address.Assign (devices);
    sink_interfaces.Add (interfaces.Get (1));
  }

//   NS_LOG_INFO ("Initialize Global Routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

  for (uint16_t i = 0; i < num_flows; i++)
  {
    AddressValue remoteAddress (InetSocketAddress (sink_interfaces.GetAddress (i, 0), port));
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcp_adu_size));
    BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());
    ftp.SetAttribute ("Remote", remoteAddress);
    ftp.SetAttribute ("SendSize", UintegerValue (tcp_adu_size));
    ftp.SetAttribute ("MaxBytes", UintegerValue (data_mbytes * 1000000));

    ApplicationContainer sourceApp = ftp.Install (sources.Get (i));
    sourceApp.Start (Seconds (rng->GetValue(0,1)*TRACE_START_TIME/10.0) );
    sourceApp.Stop (Seconds (stop_time));

    sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
    ApplicationContainer sinkApp = sinkHelper.Install (sinks.Get (i));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (stop_time));
  }

  // Set up tracing if enabled
  if (tracing)
    {
      // std::ofstream ascii;
      // Ptr<OutputStreamWrapper> ascii_wrap;
      // ascii.open ((prefix_file_name + "-ascii").c_str ());
      // ascii_wrap = new OutputStreamWrapper ((prefix_file_name + "-ascii").c_str (),
      //                                       std::ios::out);
      // stack.EnableAsciiIpv4All (ascii_wrap);

      Simulator::Schedule (Seconds (TRACE_START_TIME), &TraceCwnd, prefix_file_name + "-cwnd.data");
      // Simulator::Schedule (Seconds (TRACE_START_TIME), &TraceSsThresh, prefix_file_name + "-ssth.data");
      Simulator::Schedule (Seconds (TRACE_START_TIME), &TraceRtt, prefix_file_name + "-rtt.data");
      // Simulator::Schedule (Seconds (TRACE_START_TIME), &TraceRto, prefix_file_name + "-rto.data");
      // Simulator::Schedule (Seconds (TRACE_START_TIME), &TraceNextTx, prefix_file_name + "-next-tx.data");
      // Simulator::Schedule (Seconds (TRACE_START_TIME), &TraceInFlight, prefix_file_name + "-inflight.data");
      // Simulator::Schedule (Seconds (TRACE_START_TIME), &TraceNextRx, prefix_file_name + "-next-rx.data");
    }

  if (pcap)
    {
      BottleneckLink.EnablePcapAll (prefix_file_name, true);
      AccessLink.EnablePcapAll (prefix_file_name, true);
    }

  // Flow monitor
  FlowMonitorHelper flowHelper;
  if (flow_monitor)
    {
      flowHelper.InstallAll ();
    }

  Simulator::Stop (Seconds (stop_time*2));
  Simulator::Run ();

  if (flow_monitor)
    {
      flowHelper.SerializeToXmlFile (prefix_file_name + ".flowmonitor", true, true);
    }

  Simulator::Destroy ();

  NS_LOG_INFO("Simulation Done.");

  return 0;
}
