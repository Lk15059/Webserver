#include "log.h"
#include<iostream>
#include<tuple>
#include<utility>
#include<map>
#include<functional>
#include<time.h>
#include<string.h>

namespace sylar {

//返回级别level
const char* LogLevel::ToString(LogLevel::Level level) {
	switch (level) {
/*在要强制换行的地方输入反斜杠然后回车，系统编译的时候会自动将反斜杠下面的一行与前面的一行解释成一个语句。
这里从#define到break;的四行为一个语句，使用带name参数的宏定义使用XX替换掉case、return和break三行语句*/
#define XX(name) \
	case LogLevel::name: \
		return #name; \
		break;

		XX(DEBUG);
		XX(INFO);
		XX(WARN);
		XX(ERROR);
		XX(FATAL);
#undef XX
	default:
		return "UNKNOW";
	}
	return "UNKNOW";
}

LogEventWrap::LogEventWrap(LogEvent::ptr e)
	:m_event(e) {

}
LogEventWrap::~LogEventWrap(){
	m_event->getLogger()->log(m_event->getLevel(), m_event);
}
std::stringstream& LogEventWrap::getSS(){
	return m_event->getSS();
}


class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << event->getContent();
	}//重写基类中的纯虚函数
};

//获取级别level
class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << LogLevel::ToString(level);
	}
};

//获取启动时间
class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << event->getElapse();
	}
};

//获取名字
class NameFormatItem : public LogFormatter::FormatItem {
public:
    NameFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << logger->getName();
	}
};

//获取线程号
class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << event->getThreadId();
	}
};

//获取协程号
class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << event->getFiberId();
	}
};

//获取时间
class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
	DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S") //定义时间格式
	:m_format(format) {
		if(m_format.empty()){
			m_format="%Y-%m-%d %H:%M:%S";
		}
	}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		struct tm tm;
		time_t time=event->getTime();
		localtime_r(&time,&tm);
		char buf[64];
		strftime(buf,sizeof(buf),m_format.c_str(),&tm);
		os << buf;
	}
private:
	std::string m_format;
};

//获取文件名
class FilenameFormatItem : public LogFormatter::FormatItem {
public:
    FilenameFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << event->getFile();
	}
};

//获取行号
class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << event->getLine();
	}
};

//换行
class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << std::endl;
	}
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
	StringFormatItem(const std::string& str) :m_string(str) {}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << m_string;
	}
private:
	std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
	TabFormatItem(const std::string& str=""){}
	void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
		os << "\t";
	}
private:
	std::string m_string;
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level
            ,const char* file, int32_t line, uint32_t elapse
            ,uint32_t thread_id, uint32_t fiber_id, uint64_t time)
   :m_file(file)
   ,m_line(line)
   ,m_elapse(elapse)
   ,m_threadId(thread_id)
   ,m_fiberId(fiber_id)
   ,m_time(time)
   ,m_logger(logger)
   ,m_level(level){
}

void LogEvent::format(const char* fmt,...){
	va_list al;
	va_start(al,fmt);
	format(fmt,al);
	va_end(al);
}

void LogEvent::format(const char* fmt,va_list al){
	char* buf = nullptr;
	int len = vasprintf(&buf,fmt,al);//分配内存成功则返回长度，失败返回-1
	if(len!=-1){
		m_ss << std::string(buf,len);
		free(buf);
	}
}


Logger::Logger(const std::string& name):m_name(name),m_level(LogLevel::DEBUG) {
	//初始化为最常用的日志格式
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
	//时间 线程号 协程号 日志级别 文件名 行号 日志内容
}

void Logger::addAppender(LogAppender::ptr appender) {
    if(!appender->getFormatter()){
        appender->setFormatter(m_formatter);
    }
	m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
	//遍历列表m_appenders来删除指定appender
	for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
		if (*it == appender) {
			m_appenders.erase(it);
			break;
		}
	}
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
	if (level >= m_level) {
        auto self= shared_from_this();//智能指针self与event共享该Logger类对象的所有权
		for (auto& i : m_appenders) {
			i->log(self,level, event);//调用LogAppender类的log函数
		}
	}
}

void Logger::debug(LogEvent::ptr event) {
	log(LogLevel::DEBUG, event);
}
void Logger::info(LogEvent::ptr event) {
	log(LogLevel::INFO, event);
}
void Logger::warn(LogEvent::ptr event) {
	log(LogLevel::WARN, event);
}
void Logger::error(LogEvent::ptr event) {
	log(LogLevel::ERROR, event);
}
void Logger::fatal(LogEvent::ptr event) {
	log(LogLevel::FATAL, event);
}


FileLogAppender::FileLogAppender(const std::string& filename):m_filename(filename) {
	reopen();
}

void FileLogAppender::log(Logger::ptr logger,LogLevel::Level level, LogEvent::ptr event) {
	if (level >= m_level) {
		m_filestream << m_formatter->format(logger,level,event);//将文件按照指定格式输出
	}
}

bool FileLogAppender::reopen() {
	if (m_filestream) {
		m_filestream.close();
	}//有效（文件已打开）则先关闭
	m_filestream.open(m_filename);
	return !!m_filestream;//!!作用是将非0值转成1,0仍是0
}

void StdoutLogAppender::log(Logger::ptr logger,LogLevel::Level level, LogEvent::ptr event) {
	if (level >= m_level) {
		std::cout << m_formatter->format(logger,level,event);
	}
}

LogFormatter::LogFormatter(const std::string& pattern):m_pattern(pattern) {
	init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) {
	std::stringstream ss;
	for (auto& i : m_items) {
		i->format(ss, logger,level,event);
	}
	return ss.str();//str()函数用于将stringstream流中的数据以string字符串的形式输出
}

void LogFormatter::init() {
	//日志格式解析。
	//可能的合法格式：%XXX  %XXX{XXX}  %%
	std::vector<std::tuple<std::string,std::string, int>> vec;//三元组：str,format,type
	std::string nstr;
	for (size_t i = 0; i < m_pattern.size(); ++i) {
		//不是%的情况
		if (m_pattern[i] != '%') {
			nstr.append(1, m_pattern[i]);
			continue;
		}
		//是'%%'的情况
		if ((i + 1) < m_pattern.size()) {
			if (m_pattern[i + 1] == '%') {
				nstr.append(1, '%');
				continue;
			}
		}

		//处理是一个%的情况
		size_t n = i + 1;
		int fmt_status = 0;//解析str字符串阶段
		size_t fmt_begin = 0;

		std::string str;
		std::string fmt;
		while (n < m_pattern.size()) {
			if (!fmt_status && !isalpha(m_pattern[n]) && m_pattern[n]!='{' && m_pattern[n]!='}') {
				str=m_pattern.substr(i+1,n-i-1);
				break;
			}
			if (fmt_status == 0) {
				if (m_pattern[n] == '{') {
					//表示str已确定
					str = m_pattern.substr(i + 1, n - i-1);
					//std::cout<<"*"<<str<<std::endl;
					fmt_status = 1;//解析格式阶段
					fmt_begin = n;
					++n;
					continue;
				}
			}
			else if (fmt_status == 1) {
				if (m_pattern[n] == '}') {
					//格式解析结束
					fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin-1);
					//std::cout<<"#"<<fmt<<std::endl;
					fmt_status = 0;
					++n;
					break;
				}
			}
			++n;
			if(n==m_pattern.size()){
				if(str.empty()){
					str=m_pattern.substr(i+1);
				}
			}
		}
		if (fmt_status == 0) {
			//正常情形
			if (!nstr.empty()) {
				vec.push_back(std::make_tuple(nstr, std::string(), 0));
				nstr.clear();
			}
			
			vec.push_back(std::make_tuple(str, fmt, 1));
			i = n-1;
		}
		else if (fmt_status == 1) {
			//非正常情形
			std::cout << "pattern parser error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
			vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
		}
	}

	if (!nstr.empty()) {
		vec.push_back(std::make_tuple(nstr, "", 0));
	}
	//注意这里map键值对的第二个参数使用了function类模板，其中FormatItem::ptr是被调用函数的返回值类型，const std::string& str是被调用函数的形参
	static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)>> s_format_items = {
//#define 宏名(形参列表) 字符串
#define XX(str,C) \
		{#str,[](const std::string& fmt) { return FormatItem::ptr(new C(fmt)); }}
		/*上面的#str是将#后的宏参数进行字符串转换操作，即在该参数两边加上一对双引号，使其成为字符串，
		如{"m",[](const std::string& fmt){return FormatItem::ptr(new MessageFormatItem(fmt));}*/
		XX(m,MessageFormatItem),   //%m --消息体
		XX(p, LevelFormatItem),    //%p --level
		XX(r, ElapseFormatItem),   //%r --启动后的时间
		XX(c, NameFormatItem),     //%c --日志名称
		XX(t,ThreadIdFormatItem),  //%t --线程id
		XX(n,NewLineFormatItem),   //%n --回车换行
		XX(d,DateTimeFormatItem),  //%d --时间
	    XX(f, FilenameFormatItem), //%f --文件名
		XX(l,LineFormatItem),      //%l --行号
		XX(T,TabFormatItem),
		XX(F,FiberIdFormatItem),
#undef XX
	};

	for (auto& i : vec) {
		if (std::get<2>(i) == 0) {//get<0>表示获取tuple的第0个元素
			m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
		}//预处理，若pattern是默认格式就直接将item赋值节省开销
		else {
			auto it = s_format_items.find(std::get<0>(i));
			if (it == s_format_items.end()) {//没找到
				m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
			}
			else {
				m_items.push_back(it->second(std::get<1>(i)));
			}
		}
		//std::cout << "("<< std::get<0>(i) <<") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ")"<<std::endl;
	}
	//std::cout<<m_items.size()<<std::endl;	
	
}

LoggerManager::LoggerManager(){
	m_root.reset(new Logger);
	m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));
}

Logger::ptr LoggerManager::getLogger(const std::string& name){
	auto it=m_loggers.find(name);
	return it == m_loggers.end() ? m_root : it->second;
}
}

