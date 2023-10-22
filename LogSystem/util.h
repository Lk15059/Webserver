#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include<stdio.h>
#include<pthread.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/syscall.h>
#include<stdint.h>

namespace sylar{

pid_t GetThreadId();//获取线程号
uint32_t GetFiberId();//获取协程号

}

#endif
