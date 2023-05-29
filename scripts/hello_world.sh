#!/bin/bash

# Sample Dataset
# João Pedro Vasconcelos - jpvteixeira99@gmail.com
# First Version: 2023-05-29

# colors
NC='\033[0m'
RED='\033[0;31m'
GREEN='\033[0;32m'

# Setup Environment
echo -e "${GREEN}Setting up environment...${NC}"

# building image
echo -e "⚙️  Building the custom docker image..."
echo -e "   On the first run this usually takes a while... ⏳ (~20min in a 300Mbps connection)"
sudo docker build -f scenarios/images/ns3.Dockerfile -t ns3:latest .
if [ "$?" -ne 0 ]; then
    echo -e "${RED}Error building image. Run $0 again.${NC} ❌"
    exit 1
else
    echo -e "${GREEN}Done!${NC} ✅"
fi

# Deploying Scenario
echo -e "⚙️  Deploying the scenario..."
sudo docker compose -f scenarios/hello-world.yaml up
if [ "$?" -ne 0 ]; then
    echo -e "${RED}Error deploying the scenario. ❌"
    exit 1
else
    echo -e "${GREEN}Done!${NC} ✅"
fi