#include "MysqlServer.h"

int MysqlServer::connect()
{
	mysql = mysql_init(NULL);//数据库初始化
    if(!mysql)
    {
        LOG_ERROR<<"mysql init falied";
        return -1;
    }

    mysql = mysql_real_connect(mysql,SERVER_IP, "root", "140226", "MarioDungeon", 0, NULL, 0);//连接到数据库

    if(mysql)
    {
        LOG_INFO<<"MySQL connection success";
    }
    else
    {
        LOG_WARN<<" MySQL connection failed";
        return -1;
    }
    return 0;
}
int MysqlServer::sqlQuery(const char *query)
{
	int res=mysql_query(mysql, "set names utf8");//设置查询字符集为utf8
	if(res!=0)
	{
		LOG_INFO<<"mysql_query set utf8 error";
		return -1;
	}
    res=mysql_query(mysql,query);
    if(res)
    {
        return -1;
    }
    return 0;
}
void MysqlServer::brodcast(const string& msg)
{
	for(auto it=conn_.begin();it!=conn_.end();++it)
			(*it)->send(msg);
}
string MysqlServer::Register(char* name,char* password)
{
	string Reply="";
	char query[100];
	sprintf(query, "select username from UserInfo where username = '%s'", name);
	int ret;
	ret = sqlQuery(query);
	res_ptr = mysql_store_result(mysql); //即使不需要返回值也要这样，否则会出错
	sqlrow = mysql_fetch_row(res_ptr);
	if(sqlrow)
		Reply += "The name has been registered";
	else
	{
		mysql_free_result(res_ptr);
		memset(query, 0, sizeof(query));
		sprintf(query, "insert into UserInfo values('%s', '%s',%d,%d)", name, password,0,0);
		ret = sqlQuery(query);
		if(ret == -1)
		{
			LOG_ERROR<<"insert error :"<<mysql_error(mysql);
		}
		else
		{
			//注册成功
			//新建一个用户表，用于保存用户信息
			memset(query, 0, sizeof(query));
			sprintf(query, "create table %s (friends char(30));",name);
			ret=sqlQuery(query);
			if(ret==-1)
			{
				LOG_ERROR<<"create table error :"<<mysql_error(mysql);
			}
			//把自己加入到好友表中
			memset(query, 0, sizeof(query));
			sprintf(query, "insert into %s values('%s')", name, name);
			ret=sqlQuery(query);
			if(ret==-1)
			{
				LOG_ERROR<<"insert error :"<<mysql_error(mysql);
			}
			Reply += "REGISTER SUCCESS";
			if(mysql_errno(mysql))
			{
				LOG_ERROR<<"retrive error :"<<mysql_error(mysql);
			}
		}
	}
	return Reply;
}
string MysqlServer::Login(const TcpConnectionPtr& conn,char* name,char* password)
{
	string Reply="";
	char query[100];
	sprintf(query, "select password from UserInfo where username = '%s'",name);
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	sqlrow = mysql_fetch_row(res_ptr);
	if(!sqlrow)
		Reply += "You have to register first";
	else
	{
		if(strcmp(sqlrow[0], password) == 0)
		{
			Reply += "LOG IN SUCCESS";
			std::string namestr(name);
			mysql_free_result(res_ptr);
			memset(query,0,sizeof(query));
			sprintf(query,"update UserInfo set online=%d where username='%s'",1,name);
			sqlQuery(query);
		}
		else
			Reply += "password is wrong";
		if(mysql_errno(mysql))
		{
			LOG_ERROR<<"Retrive error :"<<mysql_error(mysql);
		}
	}

	return Reply;
}
string MysqlServer::FlushRoomList(char *playerName)
{
	string Reply="";
	char query[100];
	sprintf(query,"select name,owner from rooms");
	sqlQuery(query);
	res_ptr=mysql_store_result(mysql);
	string allmsg;
	string name;
	string owner;
	int count=0;
	while((sqlrow=mysql_fetch_row(res_ptr)))
	{
		name+=sqlrow[0];
		owner+=sqlrow[1];
		//cout<<"RoomList name:"<<name<<endl;
		//cout<<"RoomList owner:"<<owner<<endl;
		allmsg=name+","+owner+",:";
		Reply+=allmsg;
		name.clear();
		owner.clear();
		allmsg.clear();
		count++;
	}
	std::cout<<"RoomsListReply: "<<Reply<<" count: "<<count<<std::endl;
	if(Reply==""&&count==0)
	{
		Reply+="rooms empty!";
	}
	else
	{
		char countstr[5];
		sprintf(countstr,"#%d#",count);
		Reply+=countstr;
	}
	mysql_free_result(res_ptr);
	return Reply;
}
string MysqlServer::CreateRoom(char* roomName,char* owner)
{
	//cout<<"CreateRoom roomName:"<<roomName<<endl;
	//cout<<"CreateRoom owner:"<<owner<<endl;
	string reply="";
	char query[100];
	sprintf(query,"select name from rooms where name='%s'",roomName);
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	sqlrow = mysql_fetch_row(res_ptr);
	if(sqlrow)
		reply+="duplicated name!";
	else
	{
		memset(query,0,sizeof(query));
		mysql_free_result(res_ptr);
		sprintf(query,"select owner from rooms where owner='%s'",owner);
		sqlQuery(query);
		res_ptr = mysql_store_result(mysql);
		sqlrow = mysql_fetch_row(res_ptr);
		if(sqlrow)
			reply+="you already have a room!";
		else
		{
			memset(query,0,sizeof(query));
			mysql_free_result(res_ptr);
			sprintf(query,"insert into rooms values('%s','%s','%s')",roomName,owner,"");
			int ret=sqlQuery(query);
			if(ret==-1)
				LOG_ERROR<<"rooms insert error :"<<mysql_error(mysql);
			else
			{
				reply+="CreateRoom done!";
				brodcast("flush RoomList");
			}
		}
	}
	return reply;
}
string MysqlServer::DeleteRoom(char* playerName)
{
	char query[100];
	sprintf(query,"select name from rooms where owner='%s'",playerName);
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	sqlrow = mysql_fetch_row(res_ptr);
	if(!sqlrow)
	{
		LOG_ERROR<<"Delete Room error!";
		return "No Room!";
	}
	string roomName=string(sqlrow[0]);
	memset(query,0,sizeof(query));
	mysql_free_result(res_ptr);
	sprintf(query,"delete from rooms where name='%s'",roomName.c_str());
	int ret=sqlQuery(query);
	if(ret==-1)
	{
		LOG_ERROR<<"delete rooms error :"<<mysql_error(mysql);
		return "";
	}
	else
		brodcast("flush RoomList");
	return "";
}
string MysqlServer::EnterRoom(char* roomName,char* playerName)
{
	char query[100];
	sprintf(query,"select name from rooms where owner='%s'",playerName);
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	sqlrow = mysql_fetch_row(res_ptr);
	bool haveRoom=false;
	if(sqlrow)
		haveRoom=true;
	memset(query,0,sizeof(query));
	mysql_free_result(res_ptr);
	sprintf(query,"select owner from rooms where name='%s'",roomName);
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	sqlrow = mysql_fetch_row(res_ptr);
	if(!sqlrow)
	{
		LOG_ERROR<<"rooms find error!";
		return "";
	}
	else if(haveRoom&&strcmp(sqlrow[0],playerName)!=0)
	{
		return "Enter Refused!";
	}
	if(strcmp(sqlrow[0],playerName)!=0)
	{
		memset(query,0,sizeof(query));
		mysql_free_result(res_ptr);
		sprintf(query,"update rooms set player='%s' where name='%s'",playerName,roomName);
		int ret=sqlQuery(query);
		if(ret==-1)
			LOG_ERROR<<"update rooms error :"<<mysql_error(mysql);
	}
	memset(query,0,sizeof(query));
	sprintf(query,"select owner,player from rooms where name='%s'",roomName);
	int ret=sqlQuery(query);
	if(ret==-1)
		LOG_ERROR<<"rooms name error :"<<mysql_error(mysql);
	res_ptr = mysql_store_result(mysql);
	sqlrow=mysql_fetch_row(res_ptr);
	string owner=string(sqlrow[0]);
	string player=string(sqlrow[1]);
	string re="EnterRoom"+owner+'\t'+player;
	for(int i=0;i<2;++i)
	{	
		if(string(sqlrow[i])!="")
		{
			for(auto it=RoomListnameMap_.begin();it!=RoomListnameMap_.end();++it)
			{
				if(it->second==sqlrow[i])
					(it->first)->send(re);
			}
		}
	}
	mysql_free_result(res_ptr);
	return "";
}
string MysqlServer::LeaveRoom(char *playerName)
{
	char query[100];
	sprintf(query,"select name from rooms where player='%s' or owner='%s'",playerName,playerName);
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	sqlrow = mysql_fetch_row(res_ptr);
	if(!sqlrow)
	{
		LOG_ERROR<<"roomName find error!";
		return "";
	}
	string roomName=string(sqlrow[0]);
	mysql_free_result(res_ptr);
	memset(query,0,sizeof(query));
	sprintf(query,"select owner,player from rooms where name='%s'",roomName.c_str());
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	sqlrow = mysql_fetch_row(res_ptr);
	if(!sqlrow)
	{
		LOG_ERROR<<"owner,player find error!";
		return "";
	}
	string ownerName=string(sqlrow[0]);
	string re_RoomList="LeaveRoom"+string(playerName);
	string re_ChatRoom=string(playerName)+":Leave ChatRoom";
	for(int i=0;i<2;++i)
	{	
		if(string(sqlrow[i])!="")
		{
			for(auto it=RoomListnameMap_.begin();it!=RoomListnameMap_.end();++it)
			{
				if(it->second==sqlrow[i])
					(it->first)->send(re_RoomList);
			}
			for(auto it=ChatRoomnameMap_.begin();it!=ChatRoomnameMap_.end();++it)
			{
				if(it->second==sqlrow[i])
					(it->first)->send(re_ChatRoom);
			}	
		}
	}
	cout<<"LeaveRoom Msg: ownerName "<<ownerName<<" playerName:"<<playerName<<" roomName:"<<roomName<<endl;
	if(ownerName!=string(playerName))
	{
		memset(query,0,sizeof(query));
		mysql_free_result(res_ptr);
		sprintf(query,"update rooms set player='%s' where name='%s'","",roomName.c_str());
		cout<<"LeaveRoom Msg:query "<<query<<endl;
		int ret=sqlQuery(query);
		if(ret==-1)
			LOG_ERROR<<"update rooms error :"<<mysql_error(mysql);
	}
	return "";
}
string MysqlServer::ChatRoomMessage(const TcpConnectionPtr& conn,const string& msg,const string& playerName)
{
	cout<<"ChatRoomMessage:"<<msg<<endl;
	char query[100];
	sprintf(query,"select name from rooms where owner='%s' or player='%s'",playerName.c_str(),playerName.c_str());
	sqlQuery(query);
	res_ptr = mysql_store_result(mysql);
	vector<char*> nameVec;
	vector<TcpConnectionPtr> connVec;
	while((sqlrow=mysql_fetch_row(res_ptr)))
	{
		nameVec.push_back(sqlrow[0]);
	}
	if(nameVec.size()==0)
	{
		LOG_ERROR<<"ChatRoomMessage error!";
		return "";
	}
	vector<string> strVec;
	for(int i=0;i<(int)nameVec.size();++i)
	{
		memset(query,0,sizeof(query));
		mysql_free_result(res_ptr);
		sprintf(query,"select owner,player from rooms where name='%s'",nameVec[i]);
		int ret=sqlQuery(query);
		if(ret==-1)
			LOG_ERROR<<"rooms name error :"<<mysql_error(mysql);
		res_ptr = mysql_store_result(mysql);
		sqlrow=mysql_fetch_row(res_ptr);
		strVec.push_back(string(sqlrow[0]));
		strVec.push_back(string(sqlrow[1]));
		for(int i=0;i<2;++i)
		{
			if(strVec[i]!="")
			{
				for(auto it=ChatRoomnameMap_.begin();it!=ChatRoomnameMap_.end();++it)
				{
					if(it->second==strVec[i])
						connVec.push_back(it->first);
				}
			}
		}
		strVec.clear();
	}
	for(int i=0;i<(int)connVec.size();++i)
		connVec[i]->send(msg);
	return "ChatRoomMessage";
}	
