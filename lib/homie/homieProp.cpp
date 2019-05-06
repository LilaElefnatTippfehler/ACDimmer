#include "homie.hpp"


HomieProperties::HomieProperties(string propName, string fName, bool settable,
                                 bool retained, string unit,
                                 homie::datatype datatype, string format){
        if(homie::homieId(propName)) this->propName = propName;
        this->fName = fName;
        this->settable = settable;
        this->retained = retained;
        this->unit = unit;
        this->datatype = datatype;
        this->format = format;
        this->feedBackTopic = "";
        if(HOMIE_SERIAL) Serial.println("Property Const");
}

void HomieProperties::init(PubSubClient *client, string prefix){
        string topicPrefix = prefix + "/" + this->propName;
        string topic = "";
        if(this->fName != "") {
                topic = topicPrefix + "/$name";
                client->publish(topic.c_str(),this->fName.c_str(), true);
        }
        if(this->settable) {
                topic = topicPrefix + "/$settable";
                client->publish(topic.c_str(),"true", true);
                topic = topicPrefix + "/set";
                client->subscribe(topic.c_str());
                this->feedBackTopic = topicPrefix;
        }
        if(!this->retained) {
                topic = topicPrefix + "/$retained";
                client->publish(topic.c_str(),"false", true);
        }
        if(this->unit != "") {
                topic = topicPrefix + "/$unit";
                client->publish(topic.c_str(),this->unit.c_str(), true);
        }
        if(this->datatype != homie::string_t) {
                topic = topicPrefix + "/$datatype";
                string payload = this->getDTString(this->datatype);
                client->publish(topic.c_str(),payload.c_str(), true);
        }
        topic = topicPrefix + "/$format";
        if((this->datatype == homie::enum_t) || (this->datatype == homie::color_t)) {
                client->publish(topic.c_str(),this->format.c_str(), true);
        }else{
                if(this->format != "") {
                        client->publish(topic.c_str(),this->format.c_str(), true);
                }
        }
}

string HomieProperties::getPropName(){
        return this->propName;
}

string HomieProperties::getDTString(homie::datatype type){
        string typeStr = "";
        switch (type) {
        case homie::integer_t:
                typeStr = "integer";
                break;
        case homie::float_t:
                typeStr = "float";
                break;
        case homie::boolean_t:
                typeStr = "boolean";
                break;
        case homie::string_t:
                typeStr = "string";
                break;
        case homie::enum_t:
                typeStr = "enum";
                break;
        case homie::color_t:
                typeStr = "color";
                break;
        }
        return typeStr;
}

string HomieProperties::toString(){
        return "Property Name: " + propName + " fName: " + fName + " unit: " + unit + " format: " + format;
}

string HomieProperties::getFBTopic(){
        return this->feedBackTopic;
}
