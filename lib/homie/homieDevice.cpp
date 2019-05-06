#include "homie.hpp"

HomieDevice::HomieDevice(string deviceId, string fName, string localIP, string mac,
                         string fwName, string fwVersion, string implementation,
                         string interval){
        if(homie::homieId(deviceId)) this->deviceId = deviceId;
        this->fName = fName;
        this->localIP = localIP;
        this->mac = mac;
        this->fwName = fwName;
        this->fwVersion = fwVersion;
        this->implementation = implementation;
        this->interval = interval;
}
HomieDevice::HomieDevice(){
        if(homie::homieId(deviceId)) this->deviceId = "";
        this->fName = "";
        this->localIP = "";
        this->mac = "";
        this->fwName = "";
        this->fwVersion = "";
        this->implementation = "";
        this->interval = "";
}

void HomieDevice::init(PubSubClient *client){
        if(HOMIE_SERIAL) Serial.println(this->toString().c_str());
        string topicPrefix = "homie/" + this->deviceId;
        string topic = topicPrefix + "/$state";
        client->publish(topic.c_str(),this->getStateString(homie::init).c_str(), true);
        topic = topicPrefix + "/$homie";
        client->publish(topic.c_str(),HOMIE_VERSION, true);
        topic = topicPrefix + "/$name";
        client->publish(topic.c_str(),this->fName.c_str(), true);
        topic = topicPrefix + "/$localip";
        client->publish(topic.c_str(),this->localIP.c_str(), true);
        topic = topicPrefix + "/$mac";
        client->publish(topic.c_str(),this->mac.c_str(), true);
        topic = topicPrefix + "/$fw/name";
        client->publish(topic.c_str(),this->fwName.c_str(), true);
        topic = topicPrefix + "/$fw/version";
        client->publish(topic.c_str(),this->fwVersion.c_str(), true);
        topic = topicPrefix + "/$implementation";
        client->publish(topic.c_str(),this->implementation.c_str(), true);
        topic = topicPrefix + "/$stats";
        client->publish(topic.c_str(),this->getStatsString(homie::uptime).c_str(), true);

        //topic = topicPrefix + "/$stats/interval";
        //client->publish(topic.c_str(),this->interval.c_str(), true);
        std::list<HomieNode>::iterator it;
        string nodes = "";
        for(it = this->nodes.begin(); it != this->nodes.end(); ++it) {
                nodes += it->getNodeName() + ",";
        }
        nodes.pop_back();
        topic = topicPrefix + "/$nodes";
        client->publish(topic.c_str(),nodes.c_str(), true);

        for(it = this->nodes.begin(); it != this->nodes.end(); ++it) {
                it->init(client, topicPrefix);
        }
        topic = topicPrefix + "/$state";
        client->publish(topic.c_str(),this->getStateString(homie::ready).c_str(), true);
}

void HomieDevice::addNode(HomieNode node){
        this->nodes.push_back(node);
}

string HomieDevice::getStateString(homie::state state){
        switch (state) {
        case homie::init:
                return "init";
                break;
        case homie::ready:
                return "ready";
                break;
        case homie::disconnected:
                return "disconnected";
                break;
        case homie::sleeping:
                return "sleeping";
                break;
        case homie::lost:
                return "lost";
                break;
        case homie::alert:
                return "alert";
                break;
        default:
                return "";
        }
}

void HomieDevice::sendStats(PubSubClient *client, homie::stats stats, string payload){
        string topic = "homie/" + this->deviceId + "/$stats/" + this->getStatsString(stats);
        client->publish(topic.c_str(),payload.c_str(), true);
}

string HomieDevice::getStatsString(homie::stats stats){
        switch (stats) {
        case homie::uptime:
                return "uptime";
                break;
        case homie::signal:
                return "signal";
                break;
        case homie::cputemp:
                return "cputemp";
                break;
        case homie::cpuload:
                return "cpuload";
                break;
        case homie::battery:
                return "battery";
                break;
        case homie::freeheap:
                return "freeheap";
                break;
        case homie::supply:
                return "supply";
                break;
        default:
                return "";
        }
}

string HomieDevice::getDeviceId(){
        return this->deviceId;
}

string HomieDevice::toString(){
        string output = "Device Name: " + deviceId + " fName: " + fName + " IP: " +
                        localIP + " mac: " + mac + " fw: " + fwName +
                        " fw Version: " + fwVersion + " implement: " +
                        implementation + " interval: " + interval;
        return output;
}
