#include "clclib"
#include "headers.h"

#include "MemPool.h"

using namespace std;

// User sample realisation
#include <time.h>
// global
class MyGlobal: public ExeObject {
public:
virtual Value get_field(string name) {return Value();}
virtual void set_field(string name, Value &) {}
virtual Value do_call(string name, vector <Value> &args)
{
	if(name == "fmt_date") {
		char buff[10240];
		time_t t = *((int *) &args[0]);
		strftime(buff, 10240, "%c", localtime(&t));
		return buff;
	}
	if(name == "fmt_time") {
		char buff[10240];
		time_t t = *((int *) &args[0]);
		strftime(buff, 10240, "%H:%M:%S", gmtime(&t));
		return buff;
	}
	if(name == "now") return (int) time(NULL);
	return Value();
}
};
// this
struct ThisData {
int task_status;
int start_time;
int last_upd_time;
};

class MyThis: public ExeObject {
ThisData &obj;
public:
MyThis(ThisData &o) :obj(o) {}
virtual Value get_field(string name)
{
if(name == "task_status") return obj.task_status;
if(name == "start_time") return obj.start_time;
if(name == "last_upd_time") return obj.last_upd_time;
if(name == "TS_Wating") return 0;
if(name == "TS_Running") return 1;
if(name == "TS_DoneOk") return 2;
if(name == "TS_DoneFail") return 3;
if(name == "TS_DoneUnknown") return 4;
return Value();
}
virtual void set_field(string name, Value &v)
{
if(v.get_type() != 'i') return; // Given not int? Avoid cast
if(name == "task_status") obj.task_status = *((int *) &v);
else if(name == "start_time") obj.start_time = *((int *) &v);
else if(name == "last_upd_time") obj.last_upd_time = *((int *) &v);
}
virtual Value do_call(string name, vector <Value> &args)
{
return Value();
}
};

int
main(void)
{
	ThisData data = {0, 19, 46};	
	MyThis *This = new MyThis(data); 
	MyGlobal *Global = new MyGlobal(); 
	ExeParser *parse = new ExeParser();
	ExeEval *eval = new ExeEval();

	Pool->NewPool(20971520); // 20M

	parse->compile("task_status in [TS_Wating,TS_Running] ? 1 : 0");

	//eval->exec(Global, This);
	cout << *((int *) &eval->exec(Global, This)) << endl;	
	
	//cout << eval->exec(Global, This).get_str() << endl;

#if 0
	parse->compile(""
	"task_status in [TS_Wating,TS_Running]"
	"? fmt_date(start_time)+\"(\"+fmt_time(now()-start_time)+\")\" :"
	"task_status in [TS_DoneOk, TS_DoneFail, TS_DoneUnknown]"
	"? fmt_date(start_time)+\"(\"+fmt_time(last_upd_time-start_time)+\")\""	
	": fmt_date(start_time)+\"(\"+fmt_time(last_upd_time-start_time)+"
	"\", stall for \"+fmt_time(now()-last_upd_time)+\")\""
        "");
	<< parse.compile(""
        "(task_status = (3)) in [TS_Wating,TS_Running]"
        "? fmt_date(start_time)+\"(\"+fmt_time(now()-start_time)+\")\" :"
        "task_status in [TS_DoneOk, TS_DoneFail, TS_DoneUnknown]"
        "? fmt_date(start_time)+\"(\"+fmt_time(last_upd_time-start_time)+\")\""
        ": fmt_date(start_time)+\"(\"+fmt_time(last_upd_time-start_time)+"
        "\", stall for \"+fmt_time(now()-last_upd_time)+\")\""
        "") << endl 

        << parse.compile(""
        "(task_status = (10)) in [TS_Wating,TS_Running]"
        "? fmt_date(start_time)+\"(\"+fmt_time(now()-start_time)+\")\" :"
        "task_status in [TS_DoneOk, TS_DoneFail, TS_DoneUnknown]"
        "? fmt_date(start_time)+\"(\"+fmt_time(last_upd_time-start_time)+\")\""
        ": fmt_date(start_time)+\"(\"+fmt_time(last_upd_time-start_time)+"
        "\", stall for \"+fmt_time(now()-last_upd_time)+\")\""
        "") << endl;
#endif

	Pool->CleanUp();
	
	return 0;
}
