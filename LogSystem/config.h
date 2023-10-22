#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include<memory>
#include<sstream>
#include<string>
#include<boost/lexical_cast.hpp>
#include "log.h"
#include "util.h"

namespace sylar{

//基类
class ConfigVarBase{
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    ConfigVarBase(const std::string& name,const std::string& description = "")
        :m_name(name)
        ,m_description(description){
        }
    virtual ~ConfigVarBase(){}

    const std::string& getName() const { return m_name;}
    const std::string& getDescription() const { return m_description;}

    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& val) = 0;
protected:
    std::string m_name;
    std::string m_description;
};

//派生类
/*lexical_cast是一个模板函数
template<typename T, typename Source>
T lexical_cast(const Source& arg);
将arg流式处理的结果返回到基于标准库字符串的流中，然后作为模板对象输出。如果转换失败，将引发bad_lexical_cast 异常
*/
template<class T>
class ConfigVar : public ConfigVarBase{
public:
    typedef std::shared_ptr<ConfigVar> ptr;

    ConfigVar(const std::string& name
            ,const T& default_value
            ,const std::string& description = "")
        :ConfigVarBase(name, description)
        ,m_val(default_value){
    }
 
    std::string toString() override {
        try
        {
            return boost::lexical_cast<std::string>(m_val);
        }
        catch(std::exception& e)
        {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
                << e.what() << " convert: "<< typeid(m_val).name()<<" to string";
        }
        return "";
    }
    bool fromString(const std::string& val) override {
        try
        {
            m_val = boost::lexical_cast<T>(val);
        }
        catch(std::exception& e)
        {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::fromString exception"
                << e.what() << " convert: string to "<< typeid(m_val).name();
        }
        return false;  
    }
    const T getValue() const { return m_val; }
    void setValue(const T& v) { m_val=v;}
private:
    T m_val;
};

//管理类
class Config{
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

    //查找，不存在则创建
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name,
            const T& default_value, const std::string& description = ""){   
        auto tmp = Lookup<T>(name);
        if(tmp){
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
            return tmp;
        }
        if(name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678")!=std::string::npos){
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name,default_value,description));
        s_datas[name]=v;
        return v;
    }

    //查找
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name){
        auto it = s_datas.find(name);
        if(it == s_datas.end()){
            return nullptr;
        }
        //找到则将其转换为对应格式后返回
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }
private:
    static ConfigVarMap s_datas;
};

}

#endif
