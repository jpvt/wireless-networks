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

typedef std::unordered_map<std::string, int> map_nodes;
typedef std::unordered_map<std::string, map_nodes> map_patients;
void ReceivePacketServer (Ptr<Socket> socket);

// Global Configs

// Server Socket
Ptr<Socket> serverSocket;

// // Creating Actuators
std::vector<Ptr<Socket>> oxygenActSockets;
std::vector<Ptr<Socket>> tempActSockets;
std::vector<Ptr<Socket>> cardActSockets;
std::vector<Ptr<Socket>> breathActSockets;

// // Creating Package reception at Intermediate Node
std::vector<Ptr<Socket>> intermediateSocketsServer;
std::vector<Ptr<Socket>> intermediateSocketsToBreathAct;
std::vector<Ptr<Socket>> intermediateSocketsToCardAct;
std::vector<Ptr<Socket>> intermediateSocketsToTempAct;
std::vector<Ptr<Socket>> intermediateSocketsToOxygenAct;

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

  std::string s = std::string((char*)buffer);
  std::vector<std::string> data = customSplit(s, ';');
  std::string msgType = data[0];
  std::string originNodeId = data[1];
  std::string destinationNodeId = data[2];
  std::string patientId = data[3];
  
  std::string recvNodeId = std::to_string(socket->GetNode()->GetId());
  if (recvNodeId != destinationNodeId) return;

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);



  if (msgType == "SENSOR"){
    std::string vitalParameter = data[4];
    std::string sensorReading = data[5];

    std::string currentPath = originNodeId + "-" + destinationNodeId;

    std::string log_msg = "Node " + recvNodeId + "(Intermediate Node of patient " + patientId + ") received package from Node " 
      + originNodeId + " (" + vitalParameter + "'s sensor) with reading " + sensorReading + " sending it to Node 0 (Server)..." + "\n";
    log_msg += "Current path: " + currentPath;

    std::cout << log_msg << std::endl;  
    output_file << log_msg << std::endl;
    output_file.close();

    std::string packetData = "INTERMEDIATE2SERVER;" + recvNodeId + ";0;" + patientId + ";" + vitalParameter + ";" + sensorReading + ";" + currentPath;
    Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());

    int bytesSent = intermediateSocketsServer[std::stoi(patientId)]->Send (packet);
    if (bytesSent == -1) {
      std::cout << "Error sending packet to server" << std::endl;
    }

  } else if (msgType == "SERVER") {
    std::string alert = data[4];
    std::string recvPath = data[5];

    std::string currentPath = recvPath + "-" + destinationNodeId;

    std::string log_msg = "Node " + recvNodeId + "(Intermediate Node of patient " + patientId + ") received package from Node 0 (Server) with alert " + alert 
      + ", sending it to actuator...\n";
    log_msg += "Current path: " + currentPath;

    std::cout << log_msg << std::endl;
    output_file << log_msg << std::endl;
    output_file.close();

    std::string signalType = packetSender(alert);

    if (signalType == "Oxygen") {
      // Send to Oxygen Actuator
      std::string actuatorNodeId = std::to_string(std::stoi(destinationNodeId)+4);

      std::string packetData = "INTERMEDIATE2ACT;" + destinationNodeId + ";" + actuatorNodeId + ";" + patientId + ";" + alert + ";" + currentPath;

      Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
      intermediateSocketsToOxygenAct[std::stoi(patientId)]->Send (packet);

    } else if (signalType == "Temperature") {
      // Send to Temperature Actuator
      std::string actuatorNodeId = std::to_string(std::stoi(destinationNodeId)+2);

      std::string packetData = "INTERMEDIATE2ACT;" + destinationNodeId + ";" + actuatorNodeId + ";" + patientId + ";" + alert + ";" + currentPath;

      Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
      intermediateSocketsToTempAct[std::stoi(patientId)]->Send (packet);

    } else if (signalType == "Cardiac") {
      // Send to Cardiac Actuator
      std::string actuatorNodeId = std::to_string(std::stoi(destinationNodeId)+3);

      std::string packetData = "INTERMEDIATE2ACT;" + destinationNodeId + ";" + actuatorNodeId + ";" + patientId + ";" + alert + ";" + currentPath;

      Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
      intermediateSocketsToCardAct[std::stoi(patientId)]->Send (packet);
      
    } else if (signalType == "Breath") {
      // Send to Breath Actuator
      std::string actuatorNodeId = std::to_string(std::stoi(destinationNodeId)+1);

      std::string packetData = "INTERMEDIATE2ACT;" + destinationNodeId + ";" + actuatorNodeId + ";" + patientId + ";" + alert + ";" + currentPath;

      Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
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
  std::vector<std::string> data = customSplit(s, ';');
  std::string msgType = data[0];

  if (!(msgType.find("INTERMEDIATE2SERVER") != -1)) return;

  // Create a socket for sending packets to the intermediate node
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> socketToIntermediate = Socket::CreateSocket (socket->GetNode (), tid);
  socketToIntermediate->SetAllowBroadcast(true);
  socketToIntermediate->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

  std::string originNodeId = data[1];
  std::string destinationNodeId = data[2];
  std::string patientId = data[3];
  std::string vitalParameter = data[4];
  std::string sensorReading = data[5];
  std::string recvPath = data[6];

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::string currentPath = recvPath + "-0";
  std::string log_msg = "Node 0 (Server) received package from Node " + originNodeId + " (Intermediate Node of Patient " + patientId + ") with " 
    + vitalParameter + "'s reading " + sensorReading + "\n";
  log_msg += "Current path: " + currentPath;
  
  std::cout << log_msg << std::endl;
  output_file << log_msg << std::endl;

  if (vitalParameter == "Oxygen") {
    CSVRow patient_data = getPatientData(patientId);
    float data_reading = std::stof(sensorReading);
    
    if (data_reading < patient_data["min_oxigen"].get<float>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " Above expected\nSending alert!" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " Above expected\nSending alert!" << std::endl;

      std::string packetData = "SERVER;0;" + originNodeId + ";" + patientId + ";LowOxygen;" + currentPath;

      Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
      socketToIntermediate->Send (packet);

    } else {
      std::cout << "Patient " << patientId << " oxygen levels OK!" << std::endl;
      output_file << "Patient " << patientId << " oxygen levels OK!" << std::endl;
      std::cout << "\n\n" << std::endl;
    }

  } else if (vitalParameter == "Cardiac"){
    CSVRow patient_data = getPatientData(patientId);
    int data_reading = std::stoi(sensorReading);
    if (data_reading < patient_data["card_freq_lb"].get<int>() || data_reading > patient_data["card_freq_ub"].get<int>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " out of bounds\nSending alert!" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " Above expected\nSending alert!" << std::endl;

      if (data_reading < patient_data["card_freq_lb"].get<int>()) {
        std::string packetData = "SERVER;0;" + originNodeId + ";" + patientId + ";LowCardiac;" + currentPath;
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      } else {
        std::string packetData = "SERVER;0;" + originNodeId + ";" + patientId + ";HighCardiac;" + currentPath;
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      }

    } else {
      std::cout << "Patient " << patientId << " cardiac frequency OK!" << std::endl;
      output_file << "Patient " << patientId << " cardiac frequency OK!" << std::endl;
      std::cout << "\n\n" << std::endl;
    }

  } else if (vitalParameter == "Temperature"){
    CSVRow patient_data = getPatientData(patientId);
    float data_reading = std::stof(sensorReading);
    if (data_reading < patient_data["temp_lb"].get<float>() || data_reading > patient_data["temp_ub"].get<float>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " out of bounds\nSending alert!" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " Above expected\nSending alert!" << std::endl;

      if (data_reading < patient_data["temp_lb"].get<float>() ){
        std::string packetData = "SERVER;0;" + originNodeId + ";" + patientId + ";LowTemperature;" + currentPath;
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      } else {
        std::string packetData = "SERVER;0;" + originNodeId + ";" + patientId + ";HighTemperature;" + currentPath;
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      }

    } else {
      std::cout << "Patient " << patientId << " corporal temperature OK!" << std::endl;
      output_file << "Patient " << patientId << " corporal temperature OK!" << std::endl;
      std::cout << "\n\n" << std::endl;
    }

  } else if (vitalParameter == "Breath"){
    CSVRow patient_data = getPatientData(patientId);
    int data_reading = std::stoi(sensorReading);
    if (data_reading < patient_data["breath_freq_lb"].get<int>() || data_reading > patient_data["breath_freq_ub"].get<int>()){
      std::cout << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " out of bounds\nSending alert!" << std::endl;
      output_file << "PATIENT " << patientId << " AT RISK! " << vitalParameter << " Above expected\nSending alert!" << std::endl;

      if (data_reading < patient_data["breath_freq_lb"].get<int>()) {
        std::string packetData = "SERVER;0;" + originNodeId + ";" + patientId + ";LowBreath;" + currentPath;
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      } else {
        std::string packetData = "SERVER;0;" + originNodeId + ";" + patientId + ";HighBreath;" + currentPath;
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) packetData.c_str(), packetData.length());
        socketToIntermediate->Send (packet);
      }

    } else {
      std::cout << "Patient " << patientId << " breathing frequency OK!" << std::endl;
      output_file << "Patient " << patientId << " breathing frequency OK!" << std::endl;
      std::cout << "\n\n" << std::endl;
    }
    
  }

  output_file.close();
}


void TemperatureActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  std::vector<std::string> data = customSplit(s, ';');
  std::string msgType = data[0];

  if (!(msgType.find("INTERMEDIATE2ACT") != -1)) return;

  std::string originNodeId = data[1];
  std::string destinationNodeId = data[2];
  std::string patientId = data[3];
  std::string alert = data[4];
  std::string path = data[5];
  
  std::string recvNodeId = std::to_string(socket->GetNode()->GetId());

  if (destinationNodeId != recvNodeId) return;

  std::string finalPath = path + "-" + recvNodeId;

  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::string log_msg = "Node " + recvNodeId + "(Temperature's actuator Node of patient "  + patientId + ") received alert: " + alert + "\n";
  log_msg += "Final Path: " + finalPath;
  std::cout << log_msg << std::endl;
  output_file << log_msg << std::endl;

  // Take action based on received signal
  if (alert.find("HighTemperature") != -1) {
    std::cout << "Applying medication to reduce corporal temperature!" << std::endl;
    output_file << "Applying medication to reduce corporal temperature!" << std::endl;

  } else if (alert.find("LowTemperature") != -1) {
    std::cout << "Applying medication to increase corporal temperature!" << std::endl;
    output_file << "Applying medication to increase corporal temperature!" << std::endl;

  } else {
    std::cout << "Error! Command unknown at Temperature Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Temperature Actuator! Notifying nurse..." << std::endl;
  }
  std::cout << "\n\n" << std::endl;
  output_file.close();
}

void CardiacActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  std::vector<std::string> data = customSplit(s, ';');
  std::string msgType = data[0];

  if (!(msgType.find("INTERMEDIATE2ACT") != -1)) return;

  std::string originNodeId = data[1];
  std::string destinationNodeId = data[2];
  std::string patientId = data[3];
  std::string alert = data[4];
  std::string path = data[5];
  
  std::string recvNodeId = std::to_string(socket->GetNode()->GetId());

  if (destinationNodeId != recvNodeId) return;

  std::string finalPath = path + "-" + recvNodeId;
  
  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::string log_msg = "Node " + recvNodeId + "(Cardiac's actuator Node of patient "  + patientId + ") received alert: " + alert + "\n";
  log_msg += "Final Path: " + finalPath;
  std::cout << log_msg << std::endl;
  output_file << log_msg << std::endl;

  // Take action based on received signal
  if (alert.find("HighCardiac") != -1) {
    std::cout << "Applying medication to reduce cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to reduce cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;

  } else if (alert.find("LowCardiac") != -1) {
    std::cout << "Applying medication to increase cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to increase cardiac frequency! \nEmiting alert to nurses on call..." << std::endl;

  } else {
    std::cout << "Error! Command unknown at Cardiac Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Cardiac Actuator! Notifying nurse..." << std::endl;
  }
  std::cout << "\n\n" << std::endl;
  output_file.close();
}

void BreathActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  std::vector<std::string> data = customSplit(s, ';');
  std::string msgType = data[0];

  if (!(msgType.find("INTERMEDIATE2ACT") != -1)) return;

  std::string originNodeId = data[1];
  std::string destinationNodeId = data[2];
  std::string patientId = data[3];
  std::string alert = data[4];
  std::string path = data[5];
  
  std::string recvNodeId = std::to_string(socket->GetNode()->GetId());

  if (destinationNodeId != recvNodeId) return;

  std::string finalPath = path + "-" + recvNodeId;
  
  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::string log_msg = "Node " + recvNodeId + "(Breath's actuator Node of patient "  + patientId + ") received alert: " + alert + "\n";
  log_msg += "Final Path: " + finalPath;
  std::cout << log_msg << std::endl;
  output_file << log_msg << std::endl;

  // Take action based on received signal
  if (alert.find("HighBreath") != -1) {
    std::cout << "Applying medication to reduce breathing frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to reduce breathing frequency! \nEmiting alert to nurses on call..." << std::endl;

  } else if (alert.find("LowBreath") != -1) {
    std::cout << "Applying medication to increase breathing frequency! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to increase breathing frequency! \nEmiting alert to nurses on call..." << std::endl;

  } else {
    std::cout << "Error! Command unknown at Breath Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Breath Actuator! Notifying nurse..." << std::endl;
  }
  
  std::cout << "\n\n" << std::endl;
  output_file.close();
}

void OxygenActuatorAction (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  std::vector<std::string> data = customSplit(s, ';');
  std::string msgType = data[0];

  if (!(msgType.find("INTERMEDIATE2ACT") != -1)) return;

  std::string originNodeId = data[1];
  std::string destinationNodeId = data[2];
  std::string patientId = data[3];
  std::string alert = data[4];
  std::string path = data[5];
  
  std::string recvNodeId = std::to_string(socket->GetNode()->GetId());

  if (destinationNodeId != recvNodeId) return;

  std::string finalPath = path + "-" + recvNodeId;
  
  std::string filename ("scratch/hospital_logs.txt");
  std::ofstream output_file;
  output_file.open(filename.data(), std::ios_base::app);

  std::string log_msg = "Node " + recvNodeId + "(Oxygen's actuator Node of patient "  + patientId + ") received alert: " + alert + "\n";
  log_msg += "Final Path: " + finalPath;
  std::cout << log_msg << std::endl;
  output_file << log_msg << std::endl;

  // Take action based on received signal
  if (alert.find("LowOxygen") != -1) {
    std::cout << "Applying medication to increase oxygen levels! \nEmiting alert to nurses on call..." << std::endl;
    output_file << "Applying medication to increase oxygen levels! \nEmiting alert to nurses on call..." << std::endl;

  } else {
    std::cout << "Error! Command unknown at Oxygen Actuator! Notifying nurse..." << std::endl;
    output_file << "Error! Command unknown at Oxygen Actuator! Notifying nurse..." << std::endl;
  }
  
  std::cout << "\n\n" << std::endl;
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
    std::vector<std::string> data = customSplit(s, ';');
    std::string origin_node_id = data[1];
    std::string destination_node_id = data[2];
    std::string patient_id = data[3];
    std::string vital_parameter = data[4];
    std::string sensor_reading = data[5];

    std::string filename ("scratch/hospital_logs.txt");
    std::ofstream output_file;
    output_file.open(filename.data(), std::ios_base::app);
    
    std::string log_msg =  "Node " + origin_node_id 
      + "(" + vital_parameter + "'s sensor of Patient "+ patient_id + ") sending reading: " + sensor_reading
      + " to Node " + destination_node_id + " (Intermediate Node)";

    std::cout << log_msg << std::endl;
    output_file << log_msg << std::endl;
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
  std::cout << "\n---------- Patients Nodes ----------" << std::endl;
  map_patients patients_node_ids;
  for (int i = 0; i < n_rows; i++){
    map_nodes patient_nodes;
    std::cout << "# Patient " << i << std::endl;
    std::cout << " * Breath Sensor Node Id: " << breathNodes[i]->GetId() << std::endl;
    std::cout << " * Temperature Sensor Node Id: " << tempNodes[i]->GetId() << std::endl;
    std::cout << " * Cardiac Sensor Node Id: " << cardNodes[i]->GetId() << std::endl;
    std::cout << " * Oxygen Sensor Node Id: " << oxigenNodes[i]->GetId() << std::endl;
    
    // Add Sensor Nodes
    patientsFlow.Add(breathNodes[i]);
    patientsFlow.Add(tempNodes[i]);
    patientsFlow.Add(cardNodes[i]);
    patientsFlow.Add(oxigenNodes[i]);

    patient_nodes["breath"] = breathNodes[i]->GetId();
    patient_nodes["temp"] = tempNodes[i]->GetId();
    patient_nodes["card"] = cardNodes[i]->GetId();
    patient_nodes["oxygen"] = oxigenNodes[i]->GetId();

    std::cout << " * Intermediate Node Id: " << intermediateNodes[i]->GetId() << std::endl;
    // Add Intermediate Nodes
    patientsFlow.Add(intermediateNodes[i]);
    patient_nodes["intermediate"] = intermediateNodes[i]->GetId();

    std::cout << " * Breath Actuator Node Id: " << breathActNodes[i]->GetId() << std::endl;
    std::cout << " * Temperature Actuator Node Id: " << tempActNodes[i]->GetId() << std::endl;
    std::cout << " * Cardiac Actuator Node Id: " << cardActNodes[i]->GetId() << std::endl;
    std::cout << " * Oxygen Actuator Node Id: " << oxygenActNodes[i]->GetId() << std::endl;
    // Add Actuator Nodes
    patientsFlow.Add(breathActNodes[i]);
    patientsFlow.Add(tempActNodes[i]);
    patientsFlow.Add(cardActNodes[i]);
    patientsFlow.Add(oxygenActNodes[i]);

    patient_nodes["breathAct"] = breathActNodes[i]->GetId();
    patient_nodes["tempAct"] = tempActNodes[i]->GetId();
    patient_nodes["cardAct"] = cardActNodes[i]->GetId();
    patient_nodes["oxygenAct"] = oxygenActNodes[i]->GetId();

    patients_node_ids[std::to_string(i)] = patient_nodes;
  }
   serverId = n_rows;
  
  std::cout << "# Server Node Id: " << serverNode->GetId() << std::endl;
  // Add Server Node
  patientsFlow.Add(serverNode);
  std::cout << "---------- Patients Nodes ----------\n" << std::endl;

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
    // Breath Actuators
    breathActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (breathActId), actTid));
    InetSocketAddress breathActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    breathActSockets[i]->Bind(breathActAddress);
    breathActSockets[i]->SetRecvCallback (MakeCallback (&OxygenActuatorAction));

    // Cardiac Actuators
    cardActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (cardActId), actTid));
    InetSocketAddress cardActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    cardActSockets[i]->Bind(cardActAddress);
    cardActSockets[i]->SetRecvCallback (MakeCallback (&CardiacActuatorAction));

    // Temperature Actuators
    tempActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (tempActId), actTid));
    InetSocketAddress tempActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    tempActSockets[i]->Bind(tempActAddress);
    tempActSockets[i]->SetRecvCallback (MakeCallback (&TemperatureActuatorAction));

    // Oxygen Actuators
    oxygenActSockets.push_back(Socket::CreateSocket (patientsFlow.Get (oxygenActId), actTid));
    InetSocketAddress oxygenActAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);
    oxygenActSockets[i]->Bind(oxygenActAddress);
    oxygenActSockets[i]->SetRecvCallback (MakeCallback (&BreathActuatorAction));

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
    intermediateSocketsToBreathAct[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    intermediateSocketsToCardAct[i]->SetAllowBroadcast (true);
    intermediateSocketsToCardAct[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    intermediateSocketsToTempAct[i]->SetAllowBroadcast (true);
    intermediateSocketsToTempAct[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    intermediateSocketsToOxygenAct[i]->SetAllowBroadcast (true);
    intermediateSocketsToOxygenAct[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    intermediateSocketsServer[i]->Bind(local);
    intermediateSocketsServer[i]->SetRecvCallback (MakeCallback (&ReceivePacket));

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
    oxygenSockets[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    // Cardiac Sensors
    cardSockets.push_back(Socket::CreateSocket (patientsFlow.Get (cardId), tid));
    cardSockets[i]->SetAllowBroadcast (true);
    cardSockets[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    // Temperature Sensors
    tempSockets.push_back(Socket::CreateSocket (patientsFlow.Get (tempId), tid));
    tempSockets[i]->SetAllowBroadcast (true);
    tempSockets[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    // Breath Sensors
    breathSockets.push_back(Socket::CreateSocket (patientsFlow.Get (breathId), tid));
    breathSockets[i]->SetAllowBroadcast (true);
    breathSockets[i]->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));

    oxygenId+=9;
    tempId+=9;
    cardId+=9;
    breathId+=9;
  }

  // std::cout << "---------- Patients Flow ----------" << std::endl;
  // for (uint32_t i = 0; i < patientsFlow.GetN (); i++) {
  //   std::cout << "Node " << i << " Id is " << patientsFlow.Get (i)->GetId () << std::endl;
  // }

  std::vector<std::string> sensorNames = {"Oxygen", "Cardiac", "Temperature", "Breath"};
  int timer = 0;
  for (auto& row : sensorReader) {
    std::string patientId = row["patient"].get<std::string>();
    
    for (auto& sensor : sensorNames){
      std::ostringstream msg;

      if (sensor == "Oxygen") {
        std::string data = "SENSOR;" 
          + std::to_string(oxygenSockets[std::stoi(patientId)]->GetNode ()->GetId ()) + ";" 
          + std::to_string(patients_node_ids[patientId]["intermediate"]) + ";"
          + patientId + ";"
          + sensor + ";"
          + std::to_string(row["min_oxigen"].get<float>());
        
        msg << data << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());

        Simulator::ScheduleWithContext (oxygenSockets[std::stoi(patientId)]->GetNode ()->GetId (), 
                                        Seconds (timer*3), &GenerateTraffic,
                                        oxygenSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);

      } else if (sensor == "Cardiac"){
        std::string data = "SENSOR;" 
          + std::to_string(cardSockets[std::stoi(patientId)]->GetNode ()->GetId ()) + ";" 
          + std::to_string(patients_node_ids[patientId]["intermediate"]) + ";"
          + patientId + ";"
          + sensor + ";"
          + std::to_string(row["card_freq"].get<int>());
        
        msg << data << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (cardSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*3), &GenerateTraffic,
                                        cardSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);

      } else if (sensor == "Temperature"){
        std::string data = "SENSOR;" 
          + std::to_string(tempSockets[std::stoi(patientId)]->GetNode ()->GetId ()) + ";" 
          + std::to_string(patients_node_ids[patientId]["intermediate"]) + ";"
          + patientId + ";"
          + sensor + ";"
          + std::to_string(row["temp"].get<float>());
        
        msg << data << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (tempSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*3), &GenerateTraffic,
                                        tempSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);

      } else if (sensor == "Breath"){
        std::string data = "SENSOR;" 
          + std::to_string(breathSockets[std::stoi(patientId)]->GetNode ()->GetId ()) + ";" 
          + std::to_string(patients_node_ids[patientId]["intermediate"]) + ";"
          + patientId + ";"
          + sensor + ";"
          + std::to_string(row["breath_freq"].get<int>());
        
        msg << data << '\0';
        Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        Simulator::ScheduleWithContext (breathSockets[std::stoi(patientId)]->GetNode ()->GetId (),
                                        Seconds (timer*3), &GenerateTraffic,
                                        breathSockets[std::stoi(patientId)], packetSize, numPackets, interPacketInterval, 
                                        packet);
      }

      timer++;
    }
    timer*=2;
  }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
