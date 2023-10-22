#ifndef __SYLAR_SINGLETON_H__
#define __SYLAR_SINGLETON_H__

//单例 Singleton 是设计模式的一种，其特点是只提供唯一一个类的实例,具有全局变量的特点，在任何位置都可以通过接口获取到那个唯一实例;
//可能有多个Logger，但只能有一个日志管理器LoggerManager
namespace sylar{
template<class T, class X = void, int N = 0>
class Singleton{
public:
    static T* GetInstance(){
        static T v;
        return &v;
    }
};

template<class T, class X = void, int N=0>
class SingletonPtr{
public:
    static std::shared_ptr<T> GetInstance(){
        //new T是new返回的指针，指向新分配的内存块，它作为构造函数shared_ptr<T>的参数，获得一个指向T类型的智能指针v
        static std::shared_ptr<T> v(new T);
        return v;
    }
};

}

#endif
