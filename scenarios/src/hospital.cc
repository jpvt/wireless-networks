/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include <iostream>
#include <fstream> 
#include <chrono>
#include <thread>
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-metadata.h"

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "csv.hpp"

using namespace csv;
using namespace ns3;
using namespace std::chrono_literals;

void ShowPatientsData(std::string path) {
  CSVReader reader("scratch/patients_data.csv");

  std::cout << "--------------------- PATIENTS ---------------------------" << std::endl;
  for (auto& row : reader) {
    std::cout << "Patient (ID): " << row["patient"].get<std::string>() << std::endl;
    std::cout << "Card Freq Boundaries (BPM): (" << row["card_freq_lb"].get<int>() << ", " <<  row["card_freq_ub"].get<int>() << ")" << std::endl;
    std::cout << "Breath Freq Boundaries (BPM): (" << row["breath_freq_lb"].get<int>() << ", " <<  row["breath_freq_ub"].get<int>() << ")" << std::endl;
    std::cout << "Temperature Boundaries (ºC): (" << row["temp_lb"].get<float>() << ", " <<  row["temp_ub"].get<float>() << ")" << std::endl;
    std::cout << "Minimum Oxigen (0-1): " << row["min_oxigen"].get<float>() << std::endl;
    std::cout << "----------------------------------------------------------" << std::endl;
  }
}

auto getPatientData(std::string patientId) {
  CSVReader reader("scratch/patients_data.csv");
  for (auto& row : reader) {
    if (row["patient"].get<std::string>() == patientId) {
      return row;
    }
  }
}


// Create custom split() function.  
std::vector<std::string>  customSplit(std::string str, char separator) {
  std::vector<std::string>  strings;
  int startIndex = 0, endIndex = 0;
  for (int i = 0; i <= str.size(); i++) {
      
      // If we reached the end of the word or the end of the input.
      if (str[i] == separator || i == str.size()) {
          endIndex = i;
          std::string temp;
          temp.append(str, startIndex, endIndex - startIndex);
          strings.push_back(temp);
          startIndex = endIndex + 1;
      }
  }
  return strings;
}

NS_LOG_COMPONENT_DEFINE ("HospitalProject");

/**
 * Receive pacote no nó intermediário
 * \param socket Rx socket
 */
void ReceivePacket (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);
  
  std::cout << "Package received at intermediate node, sending to server...\n" << std::endl;
  
  output_file << "Package received at intermediate node, sending to server...\n" << std::endl;
  output_file.close();

  socket->Send (pkt);
}

std::string packetSender(std::string content) {
  std::string sensor;
  if (content.find("Oxigen") != -1) sensor = "Oxigen";
  else if (content.find("Temperature") != -1) sensor = "Temperature";
  else if (content.find("Cardiac") != -1) sensor = "Cardiac";
  else if (content.find("Breath") != -1) sensor = "Breath";

  return sensor;
}

/**
 * Recebe pacote no nó servidor
 * \param socket Rx socket
 */
void ReceivePacketServer (Ptr<Socket> socket) {
  CSVReader reader("scratch/patients_data.csv");
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);

  std::vector<std::string> data = customSplit(s, ' ');
  std::string patientId = data[0];
  std::string sensor = data[1];
  std::string reading = data[2];

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::cout << "Package from " << sensor << " sensor received at server node" << std::endl;
  output_file << "Package from " << sensor << " sensor received at server node" << std::endl;

  if (sensor == "Oxigen") {
    CSVRow patient_data = getPatientData(patientId);
    float data_reading = std::stof(reading);
    if (data_reading < patient_data["min_oxigen"].get<float>()){
      std::cout << "PATIENT " << patientId << "AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << "AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;
    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }

  } else if (sensor == "Cardiac"){
    CSVRow patient_data = getPatientData(patientId);
    int data_reading = std::stoi(reading);
    if (data_reading < patient_data["card_freq_lb"].get<int>() || data_reading > patient_data["card_freq_ub"].get<int>()){
      std::cout << "PATIENT " << patientId << "AT RISK! " << sensor << " out of bounds\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << "AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;
    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }

  } else if (sensor == "Temperature"){
    CSVRow patient_data = getPatientData(patientId);
    float data_reading = std::stof(reading);
    if (data_reading < patient_data["temp_lb"].get<float>() || data_reading > patient_data["temp_ub"].get<float>()){
      std::cout << "PATIENT " << patientId << "AT RISK! " << sensor << " out of bounds\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << "AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;
    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }

  } else if (sensor == "Breath"){
    CSVRow patient_data = getPatientData(patientId);
    int data_reading = std::stoi(reading);
    if (data_reading < patient_data["breath_freq_lb"].get<int>() || data_reading > patient_data["breath_freq_ub"].get<int>()){
      std::cout << "PATIENT " << patientId << "AT RISK! " << sensor << " out of bounds\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << "AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;
    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }
    
  }

  output_file.close();
}

/**
 * Geerate traffic
 * \param socket Tx socket
 * \param pktSize packet size
 * \param pktCount number of packets
 * \param pktInterval interval between packet generation
 */
static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval, Ptr<Packet> pkt) {
  if (pktCount > 0) {
    uint8_t *buffer = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buffer, pkt->GetSize ());
    std::string s = std::string((char*)buffer);
    std::string sensor = packetSender(s);

    std::string filename ("scratch/hospital_logs.txt");
    std::ofstream output_file;
    output_file.open(filename.data(), std::ios_base::app);
    
    std::cout << "Sending " << sensor << " reading"<< std::endl;

    output_file << "Sending " << sensor << " reading" << std::endl;
    output_file.close();

    socket->Send (pkt);
    Simulator::Schedule (pktInterval, &GenerateTraffic, socket, pktSize,pktCount - 1, pktInterval, pkt);
  } else {
    socket->Close();
  }
}



int main (int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  ShowPatientsData("scratch/patients_data.csv");
  CSVReader reader("scratch/patients_data.csv");
  CSVReader sensorReader("scratch/sensor_readings.csv");

  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;
  double interval = 1000;

  ns3::PacketMetadata::Enable();
  Time interPacketInterval = Seconds (interval);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);


  // // Nodes definition
  Ptr<Node> serverNode = CreateObject<Node>();
  std::vector<Ptr<Node>> breathNodes;
  std::vector<Ptr<Node>> tempNodes;
  std::vector<Ptr<Node>> cardNodes;
  std::vector<Ptr<Node>> oxigenNodes;
  std::vector<Ptr<Node>> intermediateNodes;

  int n_rows = 0;
  for (auto& row: reader) {
    breathNodes.push_back(CreateObject<Node>());
    tempNodes.push_back(CreateObject<Node>());
    cardNodes.push_back(CreateObject<Node>());
    oxigenNodes.push_back(CreateObject<Node>());
    intermediateNodes.push_back(CreateObject<Node>());
    n_rows++;
  }

  // // Patients Containers - 3, 8
  NodeContainer patientsFlow;
  int serverId;
  
  for (int i = 0; i < n_rows; i++){
    patientsFlow.Add(breathNodes[i]);
    patientsFlow.Add(tempNodes[i]);
    patientsFlow.Add(cardNodes[i]);
    patientsFlow.Add(oxigenNodes[i]);
    patientsFlow.Add(intermediateNodes[i]);
  }
  serverId = n_rows;
  patientsFlow.Add(serverNode);

  // // Physical Layer and Network Channel
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  wifiPhy.SetChannel (channel);
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  
  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));

  // // Connecting containers to network
  NetDeviceContainer devices;
  wifi80211p.Install (wifiPhy, wifi80211pMac, patientsFlow);

  // // Configuring each node position
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (2.0, 10.0, 0.0));
  positionAlloc->Add (Vector (2.0, 10.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (patientsFlow);

  InternetStackHelper internet;
  internet.Install (patientsFlow);

  // // Setting Network Devices Addresses
  Ipv4AddressHelper ipv4Patients;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4Patients.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interface = ipv4Patients.Assign (devices);


  // // Configuring Package reception at Server Node
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> serverSocket = Socket::CreateSocket (patientsFlow.Get (serverId), tid);
  InetSocketAddress serverLocation = InetSocketAddress (Ipv4Address::GetAny (), 80);
  serverSocket->Bind(serverLocation);
  serverSocket->SetRecvCallback (MakeCallback (&ReceivePacketServer));

  // // Configuring Package reception at Intermediate Node
  std::vector<Ptr<Socket>> intermediateSockets;
  std::vector<InetSocketAddress> intermediateRemotes;
  InetSocketAddress remoteServer = InetSocketAddress (Ipv4Address ("10.1.1.0"), 80);
  int intermediate = 4;
  for (int i = 0; i < n_rows; i++) {
    intermediateSockets.push_back(Socket::CreateSocket (patientsFlow.Get (intermediate), tid));
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);

    intermediateSockets[i]->Connect (remoteServer);
    intermediateSockets[i]->Bind(local);
    intermediateSockets[i]->SetRecvCallback (MakeCallback (&ReceivePacket));
    intermediateRemotes.push_back(InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));
    intermediate+=5;
  }

  // // Configuring Sensors
  std::vector<Ptr<Socket>> oxigenSockets;
  std::vector<Ptr<Socket>> tempSockets;
  std::vector<Ptr<Socket>> cardSockets;
  std::vector<Ptr<Socket>> breathSockets;
  int oxigenId = 3;
  int tempId = 1;
  int cardId = 2;
  int breathId = 0;
  for (int i = 0; i < n_rows; i++) {
    // Oxigen Sensors
    oxigenSockets.push_back(Socket::CreateSocket (patientsFlow.Get (oxigenId), tid));
    oxigenSockets[i]->SetAllowBroadcast (true);
    oxigenSockets[i]->Connect (intermediateRemotes[i]);
    std::cout << intermediateRemotes[i] << std::endl;

    // Cardiac Sensors
    cardSockets.push_back(Socket::CreateSocket (patientsFlow.Get (cardId), tid));
    cardSockets[i]->SetAllowBroadcast (true);
    cardSockets[i]->Connect (intermediateRemotes[i]);

    // Temperature Sensors
    tempSockets.push_back(Socket::CreateSocket (patientsFlow.Get (tempId), tid));
    tempSockets[i]->SetAllowBroadcast (true);
    tempSockets[i]->Connect (intermediateRemotes[i]);

    // Breath Sensors
    breathSockets.push_back(Socket::CreateSocket (patientsFlow.Get (breathId), tid));
    breathSockets[i]->SetAllowBroadcast (true);
    breathSockets[i]->Connect (intermediateRemotes[i]);

    oxigenId+=5;
    tempId+=5;
    cardId+=5;
    breathId+=5;
  }

  std::vector<std::string> sensorNames = {"Oxigen", "Cardiac", "Temperature", "Breath"};
  for (auto& row : sensorReader) {
    std::string patientId = row["patient"].get<std::string>();
    int timer = 0;
    for (auto& sensor : sensorNames){
      std::ostringstream msg;

      std::cout << "Patient: " << patientId << " Sensor: " << sensor << std::endl;

      if (sensor == "Oxigen") {
        msg << patientId + " Oxigen " + std::to_string(row["min_oxigen"].get<float>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());

        Simulator::ScheduleWithContext (oxigenSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        oxigenSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, packet);
      } else if (sensor == "Cardiac"){
        msg << patientId + " Cardiac " + std::to_string(row["card_freq"].get<int>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (cardSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        cardSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, packet);
      } else if (sensor == "Temperature"){
        msg << patientId + " Temperature " + std::to_string(row["temp"].get<float>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (tempSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        tempSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, packet);
      } else if (sensor == "Breath"){
        msg << patientId + " Breath " + std::to_string(row["breath_freq"].get<int>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (breathSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        breathSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, packet);
      }

      timer++;
    }
  }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
