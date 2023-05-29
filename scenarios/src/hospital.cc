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

void ReceivePacketServer (Ptr<Socket> socket);

// Global Configs

// Server Socket
Ptr<Socket> serverSocket;

// // Creating Actuators
std::vector<Ptr<Socket>> oxygenActSockets;
std::vector<InetSocketAddress> oxygenRemotes;
std::vector<Ptr<Socket>> tempActSockets;
std::vector<InetSocketAddress> tempRemotes;
std::vector<Ptr<Socket>> cardActSockets;
std::vector<InetSocketAddress> cardRemotes;
std::vector<Ptr<Socket>> breathActSockets;
std::vector<InetSocketAddress> breathRemotes;

// // Creating Package reception at Intermediate Node
std::vector<Ptr<Socket>> intermediateSocketsServer;
std::vector<Ptr<Socket>> intermediateSocketsToBreathAct;
std::vector<Ptr<Socket>> intermediateSocketsToCardAct;
std::vector<Ptr<Socket>> intermediateSocketsToTempAct;
std::vector<Ptr<Socket>> intermediateSocketsToOxygenAct;
std::vector<InetSocketAddress> intermediateRemotes;

// // Creating Sensors
std::vector<Ptr<Socket>> oxygenSockets;
std::vector<Ptr<Socket>> tempSockets;
std::vector<Ptr<Socket>> cardSockets;
std::vector<Ptr<Socket>> breathSockets;

// Creating processed packates set
std::set<uint32_t> processedPackets;

void ShowPatientsData(std::string path) {
  CSVReader reader("scratch/patients_data.csv");

  std::cout << "--------------------- PATIENTS ---------------------------" << std::endl;
  for (auto& row : reader) {
    std::cout << "Patient (ID): " << row["patient"].get<std::string>() << std::endl;
    std::cout << "Card Freq Boundaries (BPM): (" << row["card_freq_lb"].get<int>() << ", " <<  row["card_freq_ub"].get<int>() << ")" << std::endl;
    std::cout << "Breath Freq Boundaries (BPM): (" << row["breath_freq_lb"].get<int>() << ", " <<  row["breath_freq_ub"].get<int>() << ")" << std::endl;
    std::cout << "Temperature Boundaries (ºC): (" << row["temp_lb"].get<float>() << ", " <<  row["temp_ub"].get<float>() << ")" << std::endl;
    std::cout << "Minimum Oxygen (0-1): " << row["min_oxigen"].get<float>() << std::endl;
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

std::string packetSender(std::string content) {
  std::string sensor;
  if (content.find("Oxygen") != -1) sensor = "Oxygen";
  else if (content.find("Temperature") != -1) sensor = "Temperature";
  else if (content.find("Cardiac") != -1) sensor = "Cardiac";
  else if (content.find("Breath") != -1) sensor = "Breath";

  return sensor;
}

/**
 * Receive pacote no nó intermediário
 * \param socket Rx socket
 */
void ReceivePacket (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());

  uint32_t packetId = pkt->GetUid();
  if (processedPackets.find(packetId) != processedPackets.end()) return;
  processedPackets.insert(packetId);

  std::string s = std::string((char*)buffer);
  std::vector<std::string> rawData = customSplit(s, ';');
  std::string sender = rawData[0];
  std::string msg = rawData[1];

  if (sender == "SENSOR"){
    std::string filename ("scratch/hospital_logs.txt");
    std::ofstream output_file;
    output_file.open(filename.data(), std::ios_base::app);
    std::cout << "Package received at intermediate node from sensor, sending to server...\n" << std::endl;
    std::cout << s << std::endl;
    output_file << "Package received at intermediate node from sensor, sending to server...\n" << std::endl;
    output_file.close();

    std::vector<std::string> msgData = customSplit(msg, ' ');
    std::string patientId = msgData[0];
    std::string packetData = "INTERMEDIATE;" + msg;

    Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
    //InetSocketAddress remoteServer = InetSocketAddress (Ipv4Address ("10.1.1.0"), 80);
    // serverSocket->SetAllowBroadcast (true);
    // serverSocket->Bind (remoteServer);
    // serverSocket->SetRecvCallback (MakeCallback (&ReceivePacketServer));
    // int bytesSent = serverSocket->Send (packet);
    //intermediateSocketsServer[std::stoi(patientId)]->SetAllowBroadcast (true);
    //intermediateSocketsServer[std::stoi(patientId)]->Connect (remoteServer);
    int bytesSent = intermediateSocketsServer[std::stoi(patientId)]->Send (packet);
    std::cout << "Intermediate 2 Server Packet Data = " << packetData << std::endl;
    // std::cout << "Socket = " << intermediateSocketsServer[std::stoi(patientId)] << std::endl;
    std::cout << "Bytes sent = " << bytesSent << std::endl;
    // bytesSent = socket->Send(packet);
    // std::cout << "Socket = " << socket << std::endl;
    // std::cout << "Bytes sent = " << bytesSent << std::endl;
    

  } else if (sender == "SERVER") {
    std::string filename ("scratch/hospital_logs.txt");
    std::ofstream output_file;
    output_file.open(filename.data(), std::ios_base::app);
    std::cout << "Package received at intermediate node from server, sending to actuator...\n" << std::endl;
    output_file << "Package received at intermediate node from server, sending to actuator...\n" << std::endl;
    output_file.close();

    std::vector<std::string> msgData = customSplit(msg, ' ');
    std::string patientId = msgData[0];
    std::string signal = msgData[1];
    std::string signalType = packetSender(signal);

    if (signalType == "Oxygen") {
      // Send to Oxygen Actuator
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) signal.c_str(), signal.length());
      intermediateSocketsToOxygenAct[std::stoi(patientId)]->Send (packet);

    } else if (signalType == "Temperature") {
      // Send to Temperature Actuator
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) signal.c_str(), signal.length());
      intermediateSocketsToTempAct[std::stoi(patientId)]->Send (packet);

    } else if (signalType == "Cardiac") {
      // Send to Cardiac Actuator
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) signal.c_str(), signal.length());
      intermediateSocketsToCardAct[std::stoi(patientId)]->Send (packet);
      
    } else if (signalType == "Breath") {
      // Send to Breath Actuator
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) signal.c_str(), signal.length());
      intermediateSocketsToBreathAct[std::stoi(patientId)]->Send (packet);

    }

  }

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
  std::vector<std::string> rawData = customSplit(s, ';');
  if (!(rawData[0].find("INTERMEDIATE") != -1)) return;
  std::cout << "RECEIVED DATA IN SERVER = " << s << std::endl;

  // Create a socket for sending packets to the intermediate node
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> socketToIntermediate = Socket::CreateSocket (socket->GetNode (), tid);
  // Connect the socket to the intermediate node
  socketToIntermediate->SetAllowBroadcast(true);
  socketToIntermediate->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

  std::vector<std::string> data = customSplit(rawData[1], ' ');
  std::string patientId = data[0];
  std::string sensor = data[1];
  std::string reading = data[2];

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::cout << "Package from " << sensor << " sensor received at server node" << std::endl;
  std::cout << "Package from " << patientId << " patient received at server node" << std::endl;
  output_file << "Package from " << sensor << " sensor received at server node" << std::endl;

  if (sensor == "Oxygen") {
    CSVRow patient_data = getPatientData(patientId);
    float data_reading = std::stof(reading);
    
    if (data_reading < patient_data["min_oxigen"].get<float>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;

      std::string packetData = "SERVER;" + patientId + " " + "LowOxygen";
      Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
      socketToIntermediate->Send (packet);

    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }

  } else if (sensor == "Cardiac"){
    CSVRow patient_data = getPatientData(patientId);
    int data_reading = std::stoi(reading);
    if (data_reading < patient_data["card_freq_lb"].get<int>() || data_reading > patient_data["card_freq_ub"].get<int>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << sensor << " out of bounds\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;

      if (data_reading < patient_data["card_freq_lb"].get<int>()) {
        std::string packetData = "SERVER;" + patientId + " " + "LowCardiac";
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      } else {
        std::string packetData = "SERVER;" + patientId + " " + "HighCardiac";
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      }

    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }

  } else if (sensor == "Temperature"){
    CSVRow patient_data = getPatientData(patientId);
    float data_reading = std::stof(reading);
    if (data_reading < patient_data["temp_lb"].get<float>() || data_reading > patient_data["temp_ub"].get<float>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << sensor << " out of bounds\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;

      if (data_reading < patient_data["temp_lb"].get<float>() ){
        std::string packetData = "SERVER;" + patientId + " " + "LowTemperature";
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      } else {
        std::string packetData = "SERVER;" + patientId + " " + "HighTemperature";
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      }

    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }

  } else if (sensor == "Breath"){
    CSVRow patient_data = getPatientData(patientId);
    int data_reading = std::stoi(reading);
    if (data_reading < patient_data["breath_freq_lb"].get<int>() || data_reading > patient_data["breath_freq_ub"].get<int>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << sensor << " out of bounds\nSending alert!\n\n" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << sensor << " Above expected\nSending alert!\n\n" << std::endl;

      if (data_reading < patient_data["breath_freq_lb"].get<int>()) {
        std::string packetData = "SERVER;" + patientId + " " + "LowBreath";
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      } else {
        std::string packetData = "SERVER;" + patientId + " " + "HighBreath";
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      }

    } else {
      std::cout << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
      output_file << sensor << "Patient " << patientId << " levels OK!\n\n" << std::endl;
    }
    
  }

  output_file.close();
}


void TemperatureActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string rawData = std::string((char*)buffer);

  if (!(rawData.find("INTERMEDIATE2ACT") != -1)) return;

  std::vector<std::string> data = customSplit(rawData, ' ');
  std::string patientId = data[0];
  std::string signal = data[1];

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::cout << "Signal Received at Temperature Actuator: " << signal << " | Patient: " << patientId << std::endl;
  output_file << "Signal Received at Temperature Actuator: " << signal << " | Patient: " << patientId << std::endl;

  // Take action based on received signal
  if (signal == "HighTemperature") {
    std::cout << "Applying medication to reduce corporal temperature!" << std::endl;
    output_file << "Applying medication to reduce corporal temperature!" << std::endl;
  } else if (signal == "LowTemperature") {
    std::cout << "Applying medication to increase corporal temperature!" << std::endl;
    output_file << "Applying medication to increase corporal temperature!" << std::endl;
  } else {
    std::cout << "Error! Command unknown at Temperature Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Temperature Actuator! Notifying nurse..." << std::endl;
  }
  
  output_file.close();
}

void CardiacActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string rawData = std::string((char*)buffer);

  if (!(rawData.find("INTERMEDIATE2ACT") != -1)) return;

  std::vector<std::string> data = customSplit(rawData, ' ');
  std::string patientId = data[0];
  std::string signal = data[1];

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::cout << "Signal Received at Cardiac Actuator: " << signal << " | Patient: " << patientId << std::endl;
  output_file << "Signal Received at Cardiac Actuator: " << signal << " | Patient: " << patientId << std::endl;

  // Take action based on received signal
  if (signal == "HighCardiacFrequency") {
    std::cout << "Applying medication to reduce cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to reduce cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;
  } else if (signal == "LowCardiacFrequency") {
    std::cout << "Applying medication to increase cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to increase cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;
  } else {
    std::cout << "Error! Command unknown at Cardiac Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Cardiac Actuator! Notifying nurse..." << std::endl;
  }
  
  output_file.close();
}

void BreathActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string rawData = std::string((char*)buffer);

  if (!(rawData.find("INTERMEDIATE2ACT") != -1)) return;

  std::vector<std::string> data = customSplit(rawData, ' ');
  std::string patientId = data[0];
  std::string signal = data[1];

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::cout << "Signal Received at Breath Actuator: " << signal << "| Patient: " << patientId << std::endl;
  output_file << "Signal Received at Breath Actuator: " << signal << "| Patient: " << patientId << std::endl;

  // Take action based on received signal
  if (signal == "HighCardiacFrequency") {
    std::cout << "Applying medication to reduce breathing frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to reduce breathing frequency! \nEmiting alert to nurses on call..." << std::endl;
  } else if (signal == "LowCardiacFrequency") {
    std::cout << "Applying medication to increase breathing frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to increase breathing frequency! \nEmiting alert to nurses on call..." << std::endl;
  } else {
    std::cout << "Error! Command unknown at Breath Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Breath Actuator! Notifying nurse..." << std::endl;
  }
  
  output_file.close();
}

void OxygenActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string rawData = std::string((char*)buffer);

  if (!(rawData.find("INTERMEDIATE2ACT") != -1)) return;

  std::vector<std::string> data = customSplit(rawData, ' ');
  
  std::string patientId = data[0];
  std::string signal = data[1];

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::cout << "Signal Received at Oxygen Actuator: " << signal << " | Patient: " << patientId << std::endl;
  output_file << "Signal Received at Oxygen Actuator: " << signal << " | Patient: " << patientId << std::endl;

  // Take action based on received signal
  if (signal == "LowOxygen") {
    std::cout << "Applying medication to increase oxygen levels! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to increase oxygen levels! \nEmiting alert to nurses on call..." << std::endl;
  } else {
    std::cout << "Error! Command unknown at Oxygen Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Oxygen Actuator! Notifying nurse..." << std::endl;
  }
  
  output_file.close();
}

/**
 * Generate traffic
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
  ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
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

  ShowPatientsData("scratch/patients_data.csv");

  // Initialize Nodes
  // // Nodes definition
  Ptr<Node> serverNode = CreateObject<Node>();
  std::vector<Ptr<Node>> breathNodes;
  std::vector<Ptr<Node>> tempNodes;
  std::vector<Ptr<Node>> cardNodes;
  std::vector<Ptr<Node>> oxigenNodes;
  std::vector<Ptr<Node>> intermediateNodes;
  std::vector<Ptr<Node>> breathActNodes;
  std::vector<Ptr<Node>> tempActNodes;
  std::vector<Ptr<Node>> cardActNodes;
  std::vector<Ptr<Node>> oxygenActNodes;
  int n_rows = 0;
  for (auto& row: reader) {
    breathNodes.push_back(CreateObject<Node>());
    tempNodes.push_back(CreateObject<Node>());
    cardNodes.push_back(CreateObject<Node>());
    oxigenNodes.push_back(CreateObject<Node>());
    intermediateNodes.push_back(CreateObject<Node>());
    breathActNodes.push_back(CreateObject<Node>());
    tempActNodes.push_back(CreateObject<Node>());
    cardActNodes.push_back(CreateObject<Node>());
    oxygenActNodes.push_back(CreateObject<Node>());
    n_rows++;
  }

  // Initialize PatientsFlow
  // // Patients Container
  NodeContainer patientsFlow;
  int serverId;
  for (int i = 0; i < n_rows; i++){
    // Add Sensor Nodes
    patientsFlow.Add(breathNodes[i]);
    patientsFlow.Add(tempNodes[i]);
    patientsFlow.Add(cardNodes[i]);
    patientsFlow.Add(oxigenNodes[i]);

    // Add Intermediate Nodes
    patientsFlow.Add(intermediateNodes[i]);

    // Add Actuator Nodes
    patientsFlow.Add(breathActNodes[i]);
    patientsFlow.Add(tempActNodes[i]);
    patientsFlow.Add(cardActNodes[i]);
    patientsFlow.Add(oxygenActNodes[i]);
  }
   serverId = n_rows;
  
  // Add Server Node
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
  NetDeviceContainer devices =  wifi80211p.Install (wifiPhy, wifi80211pMac, patientsFlow);

  // // Configuring each node position
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (float i = 0.0; i < n_rows; i++){
    positionAlloc->Add (Vector (0.0, i, 0.0));
    positionAlloc->Add (Vector (1.0, i, 0.0));
    positionAlloc->Add (Vector (2.0, i, 0.0));
    positionAlloc->Add (Vector (3.0, i, 0.0));
    positionAlloc->Add (Vector (4.0, i, 2.0));
    positionAlloc->Add (Vector (0.0, i, 1.0));
    positionAlloc->Add (Vector (1.0, i, 1.0));
    positionAlloc->Add (Vector (2.0, i, 1.0));
    positionAlloc->Add (Vector (3.0, i, 1.0));
  }
  positionAlloc->Add (Vector (5.0, 0.0, 2.0));
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
  serverSocket = Socket::CreateSocket (patientsFlow.Get (serverId), tid);
  InetSocketAddress serverLocation = InetSocketAddress (Ipv4Address::GetAny (), 80);
  serverSocket->Bind(serverLocation);
  serverSocket->SetRecvCallback (MakeCallback (&ReceivePacketServer));

  // Configuring Actuators
  int oxygenActId = 5;
  int tempActId = 6;
  int cardActId = 7;
  int breathActId = 8;
  TypeId actTid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  for (int i = 0; i < n_rows; i++) {
    // Oxygen Actuators
    oxygenActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (oxygenActId), actTid));
    InetSocketAddress oxygenActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    oxygenActSockets[i]->Bind(oxygenActAddress);
    oxygenActSockets[i]->SetRecvCallback (MakeCallback (&OxygenActuatorAction));
    oxygenRemotes.push_back(InetSocketAddress (patientsFlow.Get (oxygenActId)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal (), 80));

    // Cardiac Actuators
    cardActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (cardActId), actTid));
    InetSocketAddress cardActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    cardActSockets[i]->Bind(cardActAddress);
    cardActSockets[i]->SetRecvCallback (MakeCallback (&CardiacActuatorAction));
    cardRemotes.push_back(InetSocketAddress (patientsFlow.Get (cardActId)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal (), 80));

    // Temperature Actuators
    tempActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (tempActId), actTid));
    InetSocketAddress tempActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    tempActSockets[i]->Bind(tempActAddress);
    tempActSockets[i]->SetRecvCallback (MakeCallback (&TemperatureActuatorAction));
    tempRemotes.push_back(InetSocketAddress (patientsFlow.Get (tempActId)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal (), 80));

    // Breath Actuators
    breathActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (breathActId), actTid));
    InetSocketAddress breathActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    breathActSockets[i]->Bind(breathActAddress);
    breathActSockets[i]->SetRecvCallback (MakeCallback (&BreathActuatorAction));
    breathRemotes.push_back(InetSocketAddress (patientsFlow.Get (breathActId)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal (), 80));

    oxygenActId+=9;
    tempActId+=9;
    cardActId+=9;
    breathActId+=9;
  }

  // Configuring Intermediate Nodes
  int intermediate = 4;
  oxygenActId = 5;
  tempActId = 6;
  cardActId = 7;
  breathActId = 8;
  InetSocketAddress remoteServer = InetSocketAddress (Ipv4Address ("10.1.1.0"), 80);
  for (int i = 0; i < n_rows; i++) {
    intermediateSocketsServer.push_back(Socket::CreateSocket (patientsFlow.Get (intermediate), tid));

    intermediateSocketsToBreathAct.push_back(Socket::CreateSocket (patientsFlow.Get (intermediate), tid));
    intermediateSocketsToCardAct.push_back(Socket::CreateSocket (patientsFlow.Get (intermediate), tid));
    intermediateSocketsToTempAct.push_back(Socket::CreateSocket (patientsFlow.Get (intermediate), tid));
    intermediateSocketsToOxygenAct.push_back(Socket::CreateSocket (patientsFlow.Get (intermediate), tid));

    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);

    intermediateSocketsServer[i]->SetAllowBroadcast (true);
    intermediateSocketsServer[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    intermediateSocketsToBreathAct[i]->SetAllowBroadcast (true);
    intermediateSocketsToBreathAct[i]->Connect (breathRemotes[i]);

    intermediateSocketsToCardAct[i]->SetAllowBroadcast (true);
    intermediateSocketsToCardAct[i]->Connect (cardRemotes[i]);

    intermediateSocketsToTempAct[i]->SetAllowBroadcast (true);
    intermediateSocketsToTempAct[i]->Connect (tempRemotes[i]);

    intermediateSocketsToOxygenAct[i]->SetAllowBroadcast (true);
    intermediateSocketsToOxygenAct[i]->Connect (oxygenRemotes[i]);

    intermediateSocketsServer[i]->Bind(local);
    intermediateSocketsServer[i]->SetRecvCallback (MakeCallback (&ReceivePacket));


    intermediateRemotes.push_back(InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    intermediate+=9;
    oxygenActId+=9;
    tempActId+=9;
    cardActId+=9;
    breathActId+=9;
  }

  // Configuring Sensor Nodes
  int oxygenId = 3;
  int tempId = 1;
  int cardId = 2;
  int breathId = 0;
  for (int i = 0; i < n_rows; i++) {
    // Oxygen Sensors
    oxygenSockets.push_back(Socket::CreateSocket (patientsFlow.Get (oxygenId), tid));
    oxygenSockets[i]->SetAllowBroadcast (true);
    oxygenSockets[i]->Connect (intermediateRemotes[i]);

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

    oxygenId+=9;
    tempId+=9;
    cardId+=9;
    breathId+=9;
  }

  std::vector<std::string> sensorNames = {"Oxygen", "Cardiac", "Temperature", "Breath"};
  int readings =0;
  for (auto& row : sensorReader) {
    std::cout << "Reading: " << readings++ << std::endl;
    std::string patientId = row["patient"].get<std::string>();
    int timer = 0;
    for (auto& sensor : sensorNames){
      std::ostringstream msg;

      if (sensor == "Oxygen") {
        msg << "SENSOR;" + patientId + " Oxygen " + std::to_string(row["min_oxigen"].get<float>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());

        Simulator::ScheduleWithContext (oxygenSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        oxygenSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);
      } else if (sensor == "Cardiac"){
        msg << "SENSOR;" + patientId + " Cardiac " + std::to_string(row["card_freq"].get<int>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (cardSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        cardSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);
      } else if (sensor == "Temperature"){
        msg << "SENSOR;" + patientId + " Temperature " + std::to_string(row["temp"].get<float>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (tempSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        tempSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);
      } else if (sensor == "Breath"){
        msg << "SENSOR;" + patientId + " Breath " + std::to_string(row["breath_freq"].get<int>()) << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (breathSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*1.5), &GenerateTraffic,
                                        breathSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);
      }

      timer++;
    }
    std::cout << "-------------------------------------------------" << std::endl;
  }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
