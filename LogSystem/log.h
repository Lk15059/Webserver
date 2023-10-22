#pragma once
#ifndef _SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include<string>
#include<stdint.h>
#include<memory>
#include<list>
#include<sstream>
#include<fstream>
#include<vector>
#include<stdarg.h>
#include<map>
#include "util.h"
#include "singleton.h"

//流式的宏定义（带参数的宏定义）
#define SYLAR_LOG_LEVEL(logger,level) \
	if(logger->getLevel() <= level) \
		sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger,level, \
			__FILE__,__LINE__,0,sylar::GetThreadId(),\
			sylar::GetFiberId(),time(0)))).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::WARN)	
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger,sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...)\
	if(logger->getLevel() <= level) \
		sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
						__FILE__, __LINE__, 0, sylar::GetThreadId(), \
				sylar::GetFiberId(), time(0)))).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger,sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()

namespace sylar {
class Logger;

//日志级别
class LogLevel {
public:
	enum Level {
		UNKNOW=0,
		DEBUG = 1,
		INFO = 2,
		WARN = 3,
		ERROR = 4,
		FATAL = 5
	};
	static const char* ToString(LogLevel::Level level);
};

//日志事件
class LogEvent {
public:
	typedef std::shared_ptr<LogEvent> ptr;//智能指针用来自动正确的销毁动态分配的对象
	LogEvent(std::shared_ptr<Logger> logger,LogLevel::Level level, const char* file,int32_t m_line
		,uint32_t elapse,uint32_t thread_id,uint32_t fiber_id, uint64_t time);

	const char* getFile() const { return m_file; }//const成员函数：返回文件名（const不允许修改调用对象）
	int32_t getLine() const { return m_line; }
	uint32_t getElapse() const { return m_elapse; }
	uint32_t getThreadId() const { return m_threadId; }
	uint32_t getFiberId() const { return m_fiberId; }
	uint64_t getTime() const { return m_time; }
	std::string getContent() const { return m_ss.str(); }
	std::shared_ptr<Logger> getLogger() const {return m_logger;}
	LogLevel::Level getLevel() const {return m_level;}

    std::stringstream& getSS(){ return m_ss;}
	void format(const char* fmt, ...);
	void format(const char* fmt,va_list al);

private:
	const char* m_file = nullptr;//文件名
	int32_t m_line = 0;          //行号
	uint32_t m_elapse = 0;       //程序启动开始到现在的毫秒数
	uint32_t m_threadId = 0;     //线程ID
	uint32_t m_fiberId = 0;      //协程ID
	uint64_t m_time = 0;             //时间戳
	std::stringstream m_ss;       //消息内容

	std::shared_ptr<Logger> m_logger;
	LogLevel::Level m_level;
};

class LogEventWrap{
public:
	LogEventWrap(LogEvent::ptr e);
	~LogEventWrap();
	LogEvent::ptr getEvent() const { return m_event;}
	std::stringstream& getSS();
private:
	LogEvent::ptr m_event;
};



//日志格式器
class LogFormatter {
public:
	typedef std::shared_ptr<LogFormatter> ptr;
	LogFormatter(const std::string& pattern);//根据pattern格式解析出不同Item

	std::string format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event );
public:
	//用于解析类别的抽象类
	class FormatItem {
	public:
		typedef std::shared_ptr<FormatItem> ptr;
		virtual ~FormatItem(){}

		virtual void format(std::ostream& os,std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)=0;//纯虚函数
	};
	void init();//解析pattern
private:
	std::string m_pattern;
	std::vector<FormatItem::ptr> m_items;//要输出的项
};

//日志输出地（抽象基类）
class LogAppender {
public:
	//ptr是指向LogAppender的智能指针模板类对象
	typedef std::shared_ptr<LogAppender> ptr;
	//将析构函数声明为虚函数，保证在最后可以正确释放
	virtual ~LogAppender() {}//由于日志输出有很多种，因此析构函数定义为虚类

	//纯虚函数的子类必须实现这一方法
	virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)=0;//纯虚函数

	void setFormatter(LogFormatter::ptr val) { m_formatter = val; }
	
	LogFormatter::ptr getFormatter() const { return m_formatter; }
	LogLevel::Level getLevel() const { return m_level; }
	void setLevel(LogLevel::Level val) { m_level=val; }
protected:
	LogLevel::Level m_level = LogLevel::DEBUG;
	LogFormatter::ptr m_formatter;//定义要输出的格式
};


//日志器
//enable_shared_from_this是一个模板类，Logger类继承enable_shared_from_this类就会继承基类的成员函数：shared_from_this，
//当Logger类对象x被一个名为p的std::shared_ptr<Logger>智能指针类对象管理时，调用Logger::shared_from_this成员函数将会返回
//一个新的std::shared_ptr<Logger>智能指针类对象，它与p共享对象x的所有权
class Logger :public std::enable_shared_from_this<Logger> {
public:
	typedef std::shared_ptr<Logger> ptr;                                              
		
	Logger(const std::string& name = "root");//为构造函数的参数提供默认值

	void log(LogLevel::Level level, LogEvent::ptr event);

	void debug(LogEvent::ptr event);//event是指向LogEvent的智能指针模板类对象
	void info(LogEvent::ptr event);
	void warn(LogEvent::ptr event);
	void error(LogEvent::ptr event);
	void fatal(LogEvent::ptr event);

	void addAppender(LogAppender::ptr appender);
	void delAppender(LogAppender::ptr appender);
	LogLevel::Level getLevel() const { return m_level; }
	void setLevel(LogLevel::Level val) { m_level = val; }

	const std::string getName() const { return m_name; }


private:
	std::string m_name;                       //日志名称
	LogLevel::Level m_level;                  //满足级别的日志才会被输出
	std::list<LogAppender::ptr> m_appenders;  //Appender集合，集合中存放的元素是指向LogAppender类的智能指针
    LogFormatter::ptr m_formatter;
};

//输出到控制台的Appender
class StdoutLogAppender :public LogAppender {
public:
	typedef std::shared_ptr<StdoutLogAppender> ptr;
	void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;//override描述是从父类重载的实现
};

//输出到文件的Appender
class FileLogAppender :public LogAppender {
public:
	typedef std::shared_ptr<FileLogAppender> ptr;
	FileLogAppender(const std::string& filename);
	void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;

	bool reopen();//重新打开文件，文件打开成功返回true
private:
	std::string m_filename;
	std::ofstream m_filestream;
};

//日志管理器
class LoggerManager{
public:
	LoggerManager();
	Logger::ptr getLogger(const std::string& name);

	void init();
	Logger::ptr getRoot() const { return m_root;}
private:
	std::map<std::string, Logger::ptr> m_loggers;
	Logger::ptr m_root;//主logger
};

typedef sylar::Singleton<LoggerManager> LoggerMgr;


}
#endif // !_SYLAR_LOG_H__
