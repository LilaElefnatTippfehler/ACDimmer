#include "homie.hpp"


//-------------------------------HOMIE NODE------------------------------------
HomieNode::HomieNode(string nodeName, string fName, string type){
        if(homie::homieId(nodeName)) this->nodeName = nodeName;
        this->fName = fName;
        this->type = type;
        if(HOMIE_SERIAL) Serial.println("HomieNode Const");
}

void HomieNode::init(PubSubClient *client, string prefix){
        string topicPrefix = prefix + "/" + this->nodeName;
        string topic = topicPrefix + "/$name";
        client->publish(topic.c_str(),this->fName.c_str(), true);
        topic = topicPrefix + "/$type";
        client->publish(topic.c_str(),this->type.c_str(), true);

        std::list<HomieProperties>::iterator it;
        string properties = "";
        for(it = this->props.begin(); it != this->props.end(); ++it) {
                properties += it->getPropName() + ",";
        }
        properties.pop_back();
        topic = topicPrefix + "/$properties";
        client->publish(topic.c_str(),properties.c_str(), true);

        for(it = this->props.begin(); it != this->props.end(); ++it) {
                it->init(client, topicPrefix);
        }
}

void HomieNode::addProp(HomieProperties prop){
        this->props.push_back(prop);
}

string HomieNode::getNodeName(){
        return this->nodeName;
}

string HomieNode::toString(){
        return "Node Name: " + nodeName + " fName: " + fName + " type: " + type;
}
//-----------------------------------------------------------------------------

//------------------------------HOMIE NODE ARRAY-------------------------------
HomieNodeArray::HomieNodeArray(string nodeName, string fName, string type, long arraySize) : HomieNode(nodeName, fName, type){
        this->arraySize = arraySize;
        if(HOMIE_SERIAL) Serial.println("HomieNodeArray Const");
}

void HomieNodeArray::init(PubSubClient *client, string prefix){
        string topicPrefix = prefix + "/" + this->nodeName;
        string topic = topicPrefix + "/$name";
        client->publish(topic.c_str(),this->fName.c_str(), true);
        topic = topicPrefix + "/$type";
        client->publish(topic.c_str(),this->type.c_str(), true);

        std::list<HomieProperties>::iterator it;
        string properties = "";
        for(it = this->props.begin(); it != this->props.end(); ++it) {
                properties += it->getPropName() + ",";
        }
        properties.pop_back();
        topic = topicPrefix + "/$properties";
        client->publish(topic.c_str(),properties.c_str(), true);

        topic = topicPrefix + "/$array";
        string arrayString = "0-"; //+ to_string(this->arraySize - 1);
        client->publish(topic.c_str(),arrayString.c_str(), true);

        for(it = this->props.begin(); it != this->props.end(); ++it) {
                it->init(client, topicPrefix);
        }

        int j = 0;
        std::list<string>::iterator it2;
        for(it2 = this->arrayNames.begin(); it2 != this->arrayNames.end(); ++it2) {
                topic = topicPrefix + "_"; //+ to_string(j) + "/$name";
                client->publish(topic.c_str(),it2->c_str(), true);
                j++;
        }
}

void HomieNodeArray::addProp(HomieProperties prop){
        this->props.push_back(prop);
}

void HomieNodeArray::addNames(string name){
        this->arrayNames.push_back(name);
}

string HomieNodeArray::getNodeName(){
        return this->nodeName + "[]";
}

string HomieNodeArray::toString(){
        return "Node Name: " + nodeName + " fName: " + fName + " type: " + type;
}
