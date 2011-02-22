static const char *RcsId = "$Id$\n$Name$";
//
// cpp 	- C++ source code file for TANGO dbapi class Database
//
// programmer 	- Andy Gotz (goetz@esrf.fr)
//
// original 	- September 2000
//
// last changed	- 5/9/2000 
//
// version 	- 1.0
//
// $Log$
// Revision 3.2.2.1  2003/09/18 14:07:41  taurel
// Fixes some bugs:
//  - Bug fix in DeviceProxy copy constructor and assignement operator
//  - Change the way how DeviceProxy::write_attribute() is coded
//  - Added DeviceAttribute ctors from "const char *"
//  - Split "str().c_str()" in two lines of code. It was the reason of some
//    problems using Windows VC6
//
// Revision 3.4  2003/09/02 13:09:09  taurel
// Add memorized attribute feature (only for SCALAR and WRITE/READ_WRITE attribute)
//
// Revision 3.3  2003/08/21 07:22:01  taurel
// - End of the implementation of the new way to transfer data for read and
//   write attributes (better use of exception)
// - Added Attribute::set_date() and Attribute::set_value_date_quality() methods
// - Added DeviceAttribute ctors from "const char *"
// - Enable writing of spectrum and image attributes
// - Many new DeviceAttribute ctors/inserters to enable easy image and spectrums
//   attribute writing
// - Attribute date automatically set in case of attribute quality factor set to INVALID
// - Change in the polling thread discarding element algo. to support case of polling
//   several cmd/atts at the same polling period with cmd/attr having a long response time
// - Take cmd/attr execution time into account in the "Data not updated since" polling
//   status string
// - Split "str().c_str()" code in two lines of code. It was the reason of some problem
//   on Windows device server
// - Add the possibility to set a cmd/attr polling as "externally triggered". Add method
//   to send trigger to the polling thread
//
// Revision 3.2  2003/05/28 14:42:55  taurel
// Add (conditionaly) autoconf generated include file
//
// Revision 3.1  2003/04/02 12:22:07  taurel
// Miscellaneous changes to :
//  - Modify TANGO_HOST env. variable syntax to be able to have several db servers
//  - Asynchronous calls are now done on device_2 or device depending on the device IDL version
//  - Timeout are bcks (omniORB 4.0.1)
//  - Fix bug when killing a device server via the kill command of the admin device
//    (Solaris only)
//  - Cleanup device server code to have only one ORB and one db connection within a server
//
// Revision 3.0  2003/03/25 16:30:34  taurel
// Change revision number to 3.0 before release 3.0.0 of Tango lib
//
// Revision 2.5  2003/03/20 08:54:53  taurel
// Updated to support asynchronous calls
//
// Revision 2.4  2003/01/09 12:00:33  taurel
// - Ported to gcc 3.2
// - Added ApiUtil::cleanup() and ApiUtil::~ApiUtil() methods
// - Replace some ORB * by ORB_ptr
// - Use CORBA::ORB::is_nil() instead of comparing to NULL
//
// Revision 2.3  2002/12/16 11:58:36  taurel
// - Change the underlying ORB fom ORBacus to omniORB
// - New method get_device_list() in Util class
// - Util::get_class_list() takes DServer device into account
// - Util::get_device_by_name() takes DSErver device into account
// - Util::get_device_list_by_class() takes DServer device into account
// - New parameter to the attribute::set_value() method to ebnable CORBA to frre memory allocated for the attribute
//
// Revision 2.2  2002/10/14 09:32:43  taurel
// Fix bugs in devapi_base.cpp file :
// - In read_attribute and read_attributes method of the DeviceProxy class
//   Do not create sequence the same way if the call is local or remote.
// - Add reconnection in the Connection::set_timeout_millis method
// - Add flags to the Connection::set_timeout_millis method
// - Fix bug in the DeviceProxy constructor when device is not marked as exported
//   in the database. The constructor was not stateless in this case.
//
// Revision 2.1  2002/08/12 12:43:23  taurel
// Fix bug in DeviceProxy::write_attributes method when writing several
// attributes in one call. (File devapi_base.cpp)
//
// Revision 2.0  2002/06/28 13:43:07  taurel
// Lot of changes since last releases :
// 	- Database object managed as a singleton per control system
// 	- Support all tango device naming syntax (using TANGO_HOST env.
//  	  variable, without env variable and non database device)
// 	- No more copy during read_attribute and command_inout
// 	- Added some missing methods
// 	- Build an exception class hierarchy
// 	- Added correct management of device time-out
// 	- Support all Tango device interface release 2 features
// 	  (data/attribute comming from polling buffer, polling related methods,
// 	   command/attribute result history)
//

#if HAVE_CONFIG_H
#include <ac_config.h>
#endif

#include <tango.h>
                                                  
using namespace CORBA;

namespace Tango
{

//-----------------------------------------------------------------------------
//
// Database::Database() - constructor to create connection to TANGO Database
//                        without arguments, host and port specified by
//                        environment variable TANGO_HOST
//			  Rem : For Windows, when the server is used as a
//				service, the TANGO_HOST environment
//				variable must be retrieved from the registry
//				from the device server exec name and instance
//				name
//
//-----------------------------------------------------------------------------

Database::Database(ORB *orb_in) : Connection(orb_in)
{
//
// get host and port from environment variable TANGO_HOST
//

	char *tango_host_env_c_str;

#ifdef WIN32
	tango_host_env_c_str = getenv(EnvVariable);
#else	
       	tango_host_env_c_str = getenv(EnvVariable);
#endif

       	if (tango_host_env_c_str == NULL)
       	{		
		TangoSys_MemStream desc;
		desc << "TANGO_HOST env. variable not set, set it and retry (e.g. TANGO_HOST=<host>:<port>)" << ends;
		ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       desc.str(),
					       (const char *)"Database::Database");
	}
	
	check_tango_host(tango_host_env_c_str);
	
	cout4 <<"Database::Database(): TANGO host " << host << " port " << port << endl;

	build_connection();		
}

#ifdef WIN32
Database::Database(ORB *orb_in,string &ds_exec_name,string &ds_inst_name) : Connection(orb_in)
{
//
// get host and port from environment variable TANGO_HOST
//
	char *tango_host_env_c_str;

	if (get_tango_host_from_reg(&tango_host_env_c_str,ds_exec_name,ds_inst_name) == -1)
		tango_host_env_c_str = NULL;

       	if (tango_host_env_c_str == NULL)
       	{		
		TangoSys_MemStream desc;
		desc << "TANGO_HOST env. variable not set, set it and retry (e.g. TANGO_HOST=<host>:<port>)" << ends;
		ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       desc.str(),
					       (const char *)"Database::Database");
	}
	
	check_tango_host(tango_host_env_c_str);
	
	delete [] tango_host_env_c_str;

	cout4 <<"Database::Database(): TANGO host " << host << " port " << port << endl;

	build_connection();
}
#endif

//+----------------------------------------------------------------------------
//
// method : 		Database::check_tango_host()
// 
// description : 	Check the TANGO_HOST environment variable syntax and
//			extract database server host(s) and port(s) from
//			it
//
// in :	tango_host_env_c_str : The TANGO_HOST env. variable value		
//
//-----------------------------------------------------------------------------

void Database::check_tango_host(char *tango_host_env_c_str)
{
	string tango_host_env(tango_host_env_c_str);
	string::size_type separator;

	db_multi_svc = false;
	separator = tango_host_env.find(',');
	if (separator != string::npos)
	{
	
//
// It is a multi db server system, extract each host and port and check 
// syntax validity (<host>:<port>,<host>:<port>)
//

		db_multi_svc = true;
		separator = 0;
		string::size_type old_sep = separator;
		string::size_type host_sep;
		while((separator = tango_host_env.find(',',separator + 1)) != string::npos)
		{
			string sub = tango_host_env.substr(old_sep,separator - old_sep);
			old_sep = separator + 1;
       			host_sep = sub.find(":");
       			if (host_sep != string::npos)
       			{
				if ((host_sep == sub.size() - 1) || (host_sep == 0))
				{		
					TangoSys_MemStream desc;
					desc << "TANGO_HOST env. variable syntax incorrect (e.g. TANGO_HOST=<host>:<port>,<host>:<port>)" << ends;
					ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       				desc.str(),
					       				(const char *)"Database::Database");
				}
				multi_db_port.push_back(sub.substr(host_sep + 1));
			}
			else
			{		
				TangoSys_MemStream desc;
				desc << "TANGO_HOST env. variable syntax incorrect (e.g. TANGO_HOST=<host>:<port>,<host>:<port>)" << ends;
				ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       		desc.str(),
					       		(const char *)"Database::Database");
			}
			multi_db_host.push_back(sub.substr(0,host_sep));
		}
		string last = tango_host_env.substr(old_sep);
       		host_sep = last.find(":");
       		if (host_sep != string::npos)
       		{
			if ((host_sep == last.size() - 1) || (host_sep == 0))
			{		
				TangoSys_MemStream desc;
				desc << "TANGO_HOST env. variable syntax incorrect (e.g. TANGO_HOST=<host>:<port>,<host>:<port>)" << ends;
				ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       				desc.str(),
					       				(const char *)"Database::Database");
			}
			multi_db_port.push_back(last.substr(host_sep + 1));
		}
		else
		{		
			TangoSys_MemStream desc;
			desc << "TANGO_HOST env. variable syntax incorrect (e.g. TANGO_HOST=<host>:<port>,<host>:<port>)" << ends;
			ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       		desc.str(),
					       		(const char *)"Database::Database");
		}
		multi_db_host.push_back(last.substr(0,host_sep));
		
		db_port = multi_db_port[0];
		db_host = multi_db_host[0];
		db_port_num = atoi(db_port.c_str());
	}
	else
	{

//
// Single database server case
//
			
       		separator = tango_host_env.find(":");
       		if (separator != string::npos)
       		{
			if ((separator == tango_host_env.size() - 1) || (separator == 0))
			{		
				TangoSys_MemStream desc;
				desc << "TANGO_HOST env. variable syntax incorrect (e.g. TANGO_HOST=<host>:<port>)" << ends;
				ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       				desc.str(),
					       				(const char *)"Database::Database");
			}
			db_port = tango_host_env.substr(separator+1);
			db_port_num = atoi(db_port.c_str());
		}
		else
		{		
			TangoSys_MemStream desc;
			desc << "TANGO_HOST env. variable syntax incorrect (e.g. TANGO_HOST=<host>:<port>)" << ends;
			ApiConnExcept::throw_exception((const char *)"API_TangoHostNotSet",
					       		desc.str(),
					       		(const char *)"Database::Database");
		}
		db_host = tango_host_env.substr(0,separator);
	}
	
	host = db_host;
	port = db_port;
	port_num = db_port_num;
	
	dbase_used = true;
	from_env_var = true;

}


//-----------------------------------------------------------------------------
//
// Database::Database(string host, int port, ORB *orb) - constructor to 
//           create connection to TANGO Database with host, port, orb specified.
//
//-----------------------------------------------------------------------------

Database::Database(string &in_host, int in_port, ORB *orb_in) : Connection(orb_in)
{
	db_multi_svc = false;
	
	host = in_host;
	db_host = host;
	
	TangoSys_MemStream o;
	o << in_port << ends;
#ifdef STRSTREAM
	char *ptr = o.str();
	port = ptr;
	delete [] ptr;
#else
	string st = o.str();
	port = st.c_str();
#endif

	db_port = port;
	
	db_port_num = in_port;
	port_num = in_port;
	from_env_var = false;
	dbase_used = true;
		
	build_connection();	
}

//-----------------------------------------------------------------------------
//
// Database::~Database() - destructor to destroy connection to TANGO Database
//
//-----------------------------------------------------------------------------

Database::~Database()
{
}

#ifdef WIN32
//+----------------------------------------------------------------------------
//
// method : 		Util::get_tango_host_from_reg()
// 
// description : 	When the server is a Win32 service, it is not
//			possible to get the env. variable TANGO_HOST
//			value. In this case, the TANGO_HOST value is 
//			stored in the WIN32 registry when the service is
//			installed and retrieved by this method.
//
//-----------------------------------------------------------------------------

long Database::get_tango_host_from_reg(char **buffer,
				       string &ds_exec_name,
				       string &ds_instance_name)
{

//
// Build the key name
//

	HKEY regHandle = HKEY_LOCAL_MACHINE;
	string keyName("SYSTEM\\CurrentControlSet\\Services\\");
	keyName = keyName + ds_exec_name + '_' + ds_instance_name;
	keyName = keyName + "\\Server";

//
// Open the key.
//

	HKEY keyHandle;
	if(::RegOpenKeyEx(regHandle,keyName.c_str(),0,KEY_ALL_ACCESS,
			  &keyHandle) != ERROR_SUCCESS)
	{
		return -1;
	}

//
// Read the value
//

	DWORD type,size;
	char tmp_buf[128];
	size = 127;
	if (::RegQueryValueEx(keyHandle,"TangoHost",NULL,&type,
			      (unsigned char *)tmp_buf,
			      &size) != ERROR_SUCCESS)
	{
		return -1;
	}

//
// Store answer in caller parameter
//

	*buffer = new char[strlen(tmp_buf) + 1];
	strcpy(*buffer,tmp_buf);

	return 0;
}
#endif

//-----------------------------------------------------------------------------
//
// Database::connect() - method to create connection to TANGO Database
//                       specifying host and port
//
//-----------------------------------------------------------------------------

void Database::build_connection()
{
	string corba_name = get_corba_name();
	connect(corba_name);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::state() - private method to return corba name for 
//
//-----------------------------------------------------------------------------

string Database::get_corba_name()
{
	string db_corbaloc;
	if (db_multi_svc == true)
	{
        	db_corbaloc = "corbaloc:iiop:1.2@";
		int nb_host = multi_db_host.size();
		for (int i = 0;i <  nb_host;i++)
		{
			db_corbaloc = db_corbaloc + multi_db_host[i];
			db_corbaloc = db_corbaloc + ":" + multi_db_port[i];
			if (i != nb_host - 1)
				db_corbaloc = db_corbaloc + ",iiop:1.2@";
		}
		db_corbaloc = db_corbaloc + "/" + DbObjName;
	}
	else
	{
		db_corbaloc = "corbaloc:iiop:";
        	db_corbaloc = db_corbaloc+host+":"+port;
		db_corbaloc = db_corbaloc+"/"+DbObjName;
	}

	return db_corbaloc;
}

//-----------------------------------------------------------------------------
//
// Database::get_info() - public method to return info about the Database
//
//-----------------------------------------------------------------------------

string Database::get_info()
{
	Any send;

	Any_var received = command_inout("DbInfo",send);
	const DevVarStringArray *db_info_list;
	received >>= db_info_list;

#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	for (unsigned int i=0; i<db_info_list->length(); i++)
	{
		ostream << (*db_info_list)[i] << endl;
	}
	ostream << ends;
	
	string ret_str = ostream.str();
#ifdef STRSTREAM
	ostream.rdbuf()->freeze(false);
#endif /* STRSTREAM */
	
	return(ret_str);
}

//-----------------------------------------------------------------------------
//
// Database::import_device() - public method to return import info for a device
//
//-----------------------------------------------------------------------------

DbDevImportInfo Database::import_device(string &dev)
{
	Any send;
	send <<= dev.c_str();
	Any_var received = command_inout("DbImportDevice",send);

	const DevVarLongStringArray *dev_import_list;
	received >>= dev_import_list;

	DbDevImportInfo dev_import;
	dev_import.name = dev;
	dev_import.ior = string((dev_import_list->svalue)[1]);
	dev_import.version = string((dev_import_list->svalue)[2]);
	dev_import.exported = dev_import_list->lvalue[0];

	return(dev_import);
}

//-----------------------------------------------------------------------------
//
// Database::export_device() - public method to export device to the Database
//
//-----------------------------------------------------------------------------

void Database::export_device(DbDevExportInfo& dev_export)
{
	Any send;
	DevVarStringArray *dev_export_list = new DevVarStringArray;
	dev_export_list->length(5);
	(*dev_export_list)[0] = string_dup(dev_export.name.c_str());
	(*dev_export_list)[1] = string_dup(dev_export.ior.c_str());
	(*dev_export_list)[2] = string_dup(dev_export.host.c_str());
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	ostream << dev_export.pid << ends;
#ifdef STRSTREAM
	(*dev_export_list)[3] = string_dup(ostream.str());
	delete [] (ostream.str());
#else
	string st = ostream.str();
	(*dev_export_list)[3] = string_dup(st.c_str());
#endif /* STRSTREAM */
	(*dev_export_list)[4] = string_dup(dev_export.version.c_str());
	send <<= dev_export_list;

	Any_var received = command_inout("DbExportDevice",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::unexport_device() - public method to unexport a device from the Database
//
//-----------------------------------------------------------------------------

void Database::unexport_device(string dev)
{
	Any send;
	send <<= string_dup(dev.c_str());
	Any_var received = command_inout("DbUnExportDevice",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::add_device() - public method to add a device to the Database
//
//-----------------------------------------------------------------------------

void Database::add_device(DbDevInfo &dev_info)
{
	Any send;
	DevVarStringArray *dev_info_list = new DevVarStringArray;
	dev_info_list->length(3);
	(*dev_info_list)[0] = string_dup(dev_info.server.c_str());
	(*dev_info_list)[1] = string_dup(dev_info.name.c_str());
	(*dev_info_list)[2] = string_dup(dev_info._class.c_str());
	send <<= dev_info_list;
	Any_var received = command_inout("DbAddDevice",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::delete_device() - public method to delete a device from the Database
//
//-----------------------------------------------------------------------------

void Database::delete_device(string dev)
{
	Any send;
	send <<= string_dup(dev.c_str());
	Any_var received = command_inout("DbDeleteDevice",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::add_server() - public method to add a server to the Database
//
//-----------------------------------------------------------------------------

void Database::add_server(string &server, DbDevInfos &dev_infos)
{
	Any send;
	DevVarStringArray *dev_info_list = new DevVarStringArray;
	dev_info_list->length(2*dev_infos.size()+1);
	(*dev_info_list)[0] = string_dup(server.c_str());
	for (unsigned int i=0; i<dev_infos.size(); i++)
	{
		(*dev_info_list)[i*2+1] = string_dup(dev_infos[i].name.c_str());
		(*dev_info_list)[i*2+2] = string_dup(dev_infos[i]._class.c_str());
	}
	send <<= dev_info_list;
	Any_var received = command_inout("DbAddServer",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::delete_server() - public method to delete a server from the Database
//
//-----------------------------------------------------------------------------

void Database::delete_server(string &server)
{
	Any send;
	send <<= string_dup(server.c_str());
	Any_var received = command_inout("DbDeleteServer",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::export_server() - public method to export a server to the Database
//
//-----------------------------------------------------------------------------

void Database::export_server(DbDevExportInfos& dev_export)
{
	Any send;
	DevVarStringArray *dev_export_list = new DevVarStringArray;
	dev_export_list->length(5*dev_export.size());
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	for (unsigned int i=0; i<dev_export.size(); i++)
	{
		(*dev_export_list)[i*5] = string_dup(dev_export[i].name.c_str());
		(*dev_export_list)[i*5+1] = string_dup(dev_export[i].ior.c_str());
		(*dev_export_list)[i*5+2] = string_dup(dev_export[i].host.c_str());
		ostream << dev_export[i].pid << ends;
#ifdef STRSTREAM
		(*dev_export_list)[i*5+3] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*dev_export_list)[i*5+3] = string_dup(st.c_str());
#endif /* STRSTREAM */
		(*dev_export_list)[i*5+4] = string_dup(dev_export[i].version.c_str());
	}
	send <<= dev_export_list;

	Any_var received = command_inout("DbExportServer",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::unexport_server() - public method to unexport a server from the Database
//
//-----------------------------------------------------------------------------

void Database::unexport_server(string &server)
{
	Any send;
	send <<= server.c_str();
	Any_var received = command_inout("DbUnExportServer",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::get_server_info() - public method to return info for a server
//
//-----------------------------------------------------------------------------

DbServerInfo Database::get_server_info(string &server)
{
	Any send;
	send <<= server.c_str();
	Any_var received = command_inout("DbGetServerInfo",send);

	const DevVarStringArray *server_info_list;
	received >>= server_info_list;

	DbServerInfo server_info;
	server_info.name = server;
	server_info.host = string((*server_info_list)[0]);
	server_info.mode = atoi((*server_info_list)[1]);
	server_info.level = atoi((*server_info_list)[2]);

	return(server_info);
}
//-----------------------------------------------------------------------------
//
// Database::get_device_property() - public method to get a device property from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::get_device_property(string dev, DbData &db_data)
{
	unsigned int i;
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(dev.c_str());
	for (i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbGetDeviceProperty",send);
	const DevVarStringArray *property_values;
	received >>= property_values;

	int n_props, index;
#ifdef STRSTREAM
	strstream iostream; 
#else
	stringstream iostream;
#endif /* STRSTREAM */
	iostream << (*property_values)[1].in();
	iostream >> n_props;
	index = 2;
	for (i=0; i<n_props; i++)
	{
		int n_values;
		db_data[i].name = (*property_values)[index].in(); index++;	
		iostream.clear(); iostream.seekp (0);  iostream.seekg (0);
		iostream << (*property_values)[index].in() << ends; 
		iostream >> n_values;
		index++;
	
		db_data[i].value_string.resize(n_values);
		if (n_values == 0)
		{
			index++; // skip dummy returned value " "
		}
		else
		{
			for (int j=0; j<n_values; j++)
			{
				db_data[i].value_string[j] = (*property_values)[index].in(); 
				index++;
			}
		}
	}
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::put_device_property() - public method to put a device property from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::put_device_property(string dev, DbData &db_data)
{
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	Any send;
	int index;
	
	DevVarStringArray *property_values = new DevVarStringArray;
	property_values->length(2); index = 2;
	(*property_values)[0] = string_dup(dev.c_str());
	ostream << db_data.size();
#ifdef STRSTREAM
	ostream << ends;
	(*property_values)[1] = string_dup(ostream.str());
	ostream.rdbuf()->freeze(false);
#else
	string st = ostream.str();
	(*property_values)[1] = string_dup(st.c_str());
#endif /* STRSTREAM */
	for (unsigned int i=0; i<db_data.size(); i++)
	{
		index++; property_values->length(index);
		(*property_values)[index-1] = string_dup(db_data[i].name.c_str());
		ostream.clear(); ostream.seekp (0);
		ostream << db_data[i].size() << ends;
		index++; property_values->length(index);
#ifdef STRSTREAM
		ostream << ends;
		(*property_values)[index-1] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*property_values)[index-1] = string_dup(st.c_str());
#endif /* STRSTREAM */
		for (int j=0; j<db_data[i].size(); j++)
		{
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i].value_string[j].c_str());
		}
	}
	send <<= property_values;

	Any_var received = command_inout("DbPutDeviceProperty",send);
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::delete_device_property() - public method to delete device properties from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::delete_device_property(string dev, DbData &db_data)
{
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(dev.c_str());
	for (unsigned int i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbDeleteDeviceProperty",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::get_device_attribute_property() - public method to get device attribute properties from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::get_device_attribute_property(string dev, DbData &db_data)
{
	unsigned int i;
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(dev.c_str());
	for (i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbGetDeviceAttributeProperty",send);
	const DevVarStringArray *property_values;
	received >>= property_values;

	int n_attribs, index;
	int i_total_props;
#ifdef STRSTREAM
	strstream iostream; 
#else
	stringstream iostream;
#endif /* STRSTREAM */
	iostream << (*property_values)[1].in();
	iostream >> n_attribs;
	index = 2;
	db_data.resize((property_values->length())/2 - 1);
	i_total_props = 0;
	for (i=0; i<n_attribs; i++)
	{
		short n_props;
		db_data[i_total_props].name = (*property_values)[index]; index++;	
		iostream.clear(); iostream.seekp(0); iostream.seekg(0);
		iostream << (*property_values)[index].in() << ends; 
		iostream >> n_props;
		db_data[i_total_props] << n_props; i_total_props++;
		index++;
		for (int j=0; j<n_props; j++)
		{
			db_data[i_total_props].name = (*property_values)[index]; 
			index++;
			db_data[i_total_props].value_string.resize(1);
			db_data[i_total_props].value_string[0] = (*property_values)[index]; 
			index++; i_total_props++;
		}
	}
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::put_device_attribute_property() - public method to put device 
//	     attribute properties into the Database
//
//-----------------------------------------------------------------------------

void Database::put_device_attribute_property(string dev, DbData &db_data)
{
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	Any send;
	int index, n_attribs ;
	
	DevVarStringArray *property_values = new DevVarStringArray;
	property_values->length(2); index = 2;
	(*property_values)[0] = string_dup(dev.c_str());
	n_attribs = 0;
	for (unsigned int i=0; i<db_data.size();)
	{
		short n_props;
		index++; property_values->length(index);
		(*property_values)[index-1] = string_dup(db_data[i].name.c_str());
		db_data[i] >> n_props;
		ostream.clear(); ostream.seekp(0);
		ostream << n_props << ends;
		index++; property_values->length(index);
#ifdef STRSTREAM
		(*property_values)[index-1] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*property_values)[index-1] = string_dup(st.c_str());
#endif /* STRSTREAM */
		for (int j=0; j<n_props; j++)
		{
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i+j+1].name.c_str());
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i+j+1].value_string[0].c_str());
		}
		i = i+n_props+1;
		n_attribs++;
	}
	ostream.clear(); ostream.seekp(0);
	ostream << n_attribs << ends;
#ifdef STRSTREAM
	(*property_values)[1] = string_dup(ostream.str());
	ostream.rdbuf()->freeze(false);
#else
	string st = ostream.str();
	(*property_values)[1] = string_dup(st.c_str());
#endif /* STRSTREAM */
	send <<= property_values;

	Any_var received = command_inout("DbPutDeviceAttributeProperty",send);
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::delete_device_attribute_property() - public method to delete device
//	     attribute properties from the Database
//
//-----------------------------------------------------------------------------

void Database::delete_device_attribute_property(string dev, DbData &db_data)
{
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	Any send;
	int index, n_attribs;
	
	DevVarStringArray *property_values = new DevVarStringArray;
	property_values->length(2);
// index = 2;
 	index = 1;
	(*property_values)[0] = string_dup(dev.c_str());
	n_attribs = 0;
	for (unsigned int i=0; i<db_data.size();)
	{
		short n_props;
		index++; property_values->length(index);
		(*property_values)[index-1] = string_dup(db_data[i].name.c_str());
		db_data[i] >> n_props;
		ostream.clear(); ostream.seekp(0);
		ostream << n_props << ends;
		index++; property_values->length(index);
#ifdef STRSTREAM
		(*property_values)[index-1] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*property_values)[index-1] = string_dup(st.c_str());
#endif /* STRSTREAM */
		for (int j=0; j<n_props; j++)
		{
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i+j+1].name.c_str());
		}
		i = i+n_props+1;
		n_attribs++;
	}
	ostream << n_attribs << ends;
#ifdef STRSTREAM
//	(*property_values)[1] = string_dup(ostream.str());
	ostream.rdbuf()->freeze(false);
#else
//	(*property_values)[1] = string_dup(ostream.str().c_str());
#endif /* STRSTREAM */
	send <<= property_values;

	Any_var received = command_inout("DbDeleteDeviceAttributeProperty",send);
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::get_class_property() - public method to get class properties from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::get_class_property(string device_class, DbData &db_data)
{
	unsigned int i;
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(device_class.c_str());
	for (i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbGetClassProperty",send);
	const DevVarStringArray *property_values;
	received >>= property_values;

	int n_props, index;
#ifdef STRSTREAM
	strstream iostream; 
#else
	stringstream iostream;
#endif /* STRSTREAM */
	iostream << (*property_values)[1].in();
	iostream >> n_props;
	index = 2;
	for (i=0; i<n_props; i++)
	{
		int n_values;
		db_data[i].name = (*property_values)[index]; index++;	
		iostream.clear(); iostream.seekp(0); iostream.seekg(0);
		iostream << (*property_values)[index].in(); index++;
		iostream >> n_values;
	
		db_data[i].value_string.resize(n_values);
		for (int j=0; j<n_values; j++)
		{

			db_data[i].value_string[j] = (*property_values)[index]; 
			index++;
		}
	}
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::put_class_property() - public method to put class properties from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::put_class_property(string device_class, DbData &db_data)
{
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	Any send;
	int index;
	
	DevVarStringArray *property_values = new DevVarStringArray;
	property_values->length(2); index = 2;
	(*property_values)[0] = string_dup(device_class.c_str());
	ostream << db_data.size() << ends;
#ifdef STRSTREAM
	(*property_values)[1] = string_dup(ostream.str());
	ostream.rdbuf()->freeze(false);
#else
	string st = ostream.str();
	(*property_values)[1] = string_dup(st.c_str());
#endif /* STRSTREAM */
	for (unsigned int i=0; i<db_data.size(); i++)
	{
		index++; property_values->length(index);
		(*property_values)[index-1] = string_dup(db_data[i].name.c_str());
		ostream.clear(); ostream.seekp (0);
		ostream << db_data[i].size() << ends;
		index++; property_values->length(index);
#ifdef STRSTREAM
		(*property_values)[index-1] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*property_values)[index-1] = string_dup(st.c_str());
#endif /* STRSTREAM */
		for (int j=0; j<db_data[i].size(); j++)
		{
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i].value_string[j].c_str());
		}
	}
	send <<= property_values;

	Any_var received = command_inout("DbPutClassProperty",send);
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::delete_class_property() - public method to delete class properties from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::delete_class_property(string device_class, DbData &db_data)
{
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(device_class.c_str());
	for (unsigned int i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbDeleteClassProperty",send);

	return;
}

//-----------------------------------------------------------------------------
//
// Database::get_class_attribute_property() - public method to get class attribute properties from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::get_class_attribute_property(string device_class, DbData &db_data)
{
	unsigned int i;
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(device_class.c_str());
	for (i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbGetClassAttributeProperty",send);
	const DevVarStringArray *property_values;
	received >>= property_values;

	int n_attribs, index;
	int i_total_props;
#ifdef STRSTREAM
	strstream iostream; 
#else
	stringstream iostream;
#endif /* STRSTREAM */
	iostream << (*property_values)[1].in();
	iostream >> n_attribs;
	index = 2;
	db_data.resize((property_values->length())/2 - 1);
	i_total_props = 0;
	for (i=0; i<n_attribs; i++)
	{
		short n_props;
		db_data[i_total_props].name = (*property_values)[index]; index++;	
		iostream.clear(); iostream.seekp(0); iostream.seekg(0);
		iostream << (*property_values)[index].in() << ends; index++;
		iostream >> n_props;
		db_data[i_total_props] << n_props; i_total_props++;
		for (int j=0; j<n_props; j++)
		{
			db_data[i_total_props].name = (*property_values)[index]; 
			index++;
			db_data[i_total_props].value_string.resize(1);
			db_data[i_total_props].value_string[0] = (*property_values)[index]; 
			index++; i_total_props++;
		}
	}
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::put_class_attribute_property() - public method to put class 
//	     attribute properties into the Database
//
//-----------------------------------------------------------------------------

void Database::put_class_attribute_property(string device_class, DbData &db_data)
{
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	Any send;
	int index, n_attribs;
	
	DevVarStringArray *property_values = new DevVarStringArray;
	property_values->length(2); index = 2;
	(*property_values)[0] = string_dup(device_class.c_str());
	n_attribs = 0;
	for (unsigned int i=0; i<db_data.size(); )
	{
		short n_props;
		index++; property_values->length(index);
		(*property_values)[index-1] = string_dup(db_data[i].name.c_str());
		db_data[i] >> n_props;
		ostream.clear(); ostream.seekp(0);
		ostream << n_props << ends;
		index++; property_values->length(index);
#ifdef STRSTREAM
		(*property_values)[index-1] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*property_values)[index-1] = string_dup(st.c_str());
#endif /* STRSTREAM */
		for (int j=0; j<n_props; j++)
		{
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i+j+1].name.c_str());
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i+j+1].value_string[0].c_str());
		}
		i = i+n_props+1;
		n_attribs++;
	}
	ostream.clear();ostream.seekp(0);
	ostream << n_attribs << ends;
#ifdef STRSTREAM
	(*property_values)[1] = string_dup(ostream.str());
	ostream.rdbuf()->freeze(false);
#else
	string st = ostream.str();
	(*property_values)[1] = string_dup(st.c_str());
#endif /* STRSTREAM */
	send <<= property_values;

	Any_var received = command_inout("DbPutClassAttributeProperty",send);
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::delete_class_attribute_property() - public method to delete class
//	     attribute properties from the Database
//
//-----------------------------------------------------------------------------

void Database::delete_class_attribute_property(string device_class, DbData &db_data)
{
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	Any send;
	int index, n_attribs;
	
	DevVarStringArray *property_values = new DevVarStringArray;
	property_values->length(2); index = 2;
	(*property_values)[0] = string_dup(device_class.c_str());
	n_attribs = 0;
	for (unsigned int i=0; i<db_data.size();)
	{
		short n_props;
		index++; property_values->length(index);
		(*property_values)[index-1] = string_dup(db_data[i].name.c_str());
		db_data[i] >> n_props;
		ostream.clear(); ostream.seekp(0);
		ostream << n_props << ends;
		index++; property_values->length(index);
#ifdef STRSTREAM
		(*property_values)[index-1] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*property_values)[index-1] = string_dup(st.c_str());
#endif /* STRSTREAM */
		for (int j=0; j<n_props; j++)
		{
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i+j+1].name.c_str());
		}
		i = i+n_props+1;
		n_attribs++;
	}
	ostream << n_attribs << ends;
#ifdef STRSTREAM
	(*property_values)[1] = string_dup(ostream.str());
	ostream.rdbuf()->freeze(false);
#else
	string st = ostream.str();
	(*property_values)[1] = string_dup(st.c_str());
#endif /* STRSTREAM */
	send <<= property_values;

	Any_var received = command_inout("DbDeleteClassAttributeProperty",send);
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::get_device_name() - public method to get device names from
//                               the Database
//
//-----------------------------------------------------------------------------

DbDatum Database::get_device_name(string &device_server, string &device_class)
{
	Any send;
	DevVarStringArray *device_server_class = new DevVarStringArray;
	device_server_class->length(2);
	(*device_server_class)[0] = string_dup(device_server.c_str());
	(*device_server_class)[1] = string_dup(device_class.c_str());
	send <<= device_server_class;

	Any_var received = command_inout("DbGetDeviceList",send);
	const DevVarStringArray *device_names;
	received >>= device_names;

	DbDatum db_datum;
	int n_devices;
	n_devices = device_names->length();
	db_datum.name = device_server;
	db_datum.value_string.resize(n_devices);
	for (int i=0; i<n_devices; i++)
	{
		db_datum.value_string[i] = (*device_names)[i]; 
	}
		
	return db_datum;
}

//-----------------------------------------------------------------------------
//
// Database::get_device_alias() - public method to get device aliases from
//                               the Database
//
//-----------------------------------------------------------------------------

DbDatum Database::get_device_alias(string  &dev)
{
	Any send;
	send <<= dev.c_str();

	Any_var received = command_inout("DbGetDeviceAliasList",send);
	const DevVarStringArray *device_alias;
	received >>= device_alias;

	DbDatum db_datum;
	int n_aliases;
	n_aliases = device_alias->length();
	db_datum.name = dev;
	db_datum.value_string.resize(n_aliases);
	for (int i=0; i<n_aliases; i++)
	{
		db_datum.value_string[i] = (*device_alias)[i]; 
	}
		
	return db_datum;
}


//-----------------------------------------------------------------------------
//
// Database::get_device_exported() - public method to get exported device names from
//                               the Database
//
//-----------------------------------------------------------------------------

DbDatum Database::get_device_exported(string &filter)
{
	Any send;
	send <<= filter.c_str();

	Any_var received = command_inout("DbGetDeviceExportedList",send);
	const DevVarStringArray *device_names;
	received >>= device_names;

	DbDatum db_datum;
	int n_devices;
	n_devices = device_names->length();
	db_datum.name = filter;
	db_datum.value_string.resize(n_devices);
	for (int i=0; i<n_devices; i++)
	{
		db_datum.value_string[i] = (*device_names)[i]; 
	}
		
	return db_datum;
}

//-----------------------------------------------------------------------------
//
// Database::get_device_member() - public method to get device members from
//                               the Database
//
//-----------------------------------------------------------------------------

DbDatum Database::get_device_member(string &wildcard)
{
	Any send;
	send <<= wildcard.c_str();

	Any_var received = command_inout("DbGetDeviceMemberList",send);
	const DevVarStringArray *device_member;
	received >>= device_member;

	DbDatum db_datum;
	int n_members;
	n_members = device_member->length();
	db_datum.name = wildcard;
	db_datum.value_string.resize(n_members);
	for (int i=0; i<n_members; i++)
	{
		db_datum.value_string[i] = (*device_member)[i]; 
	}
		
	return db_datum;
}

//-----------------------------------------------------------------------------
//
// Database::get_device_family() - public method to get device familys from
//                               the Database
//
//-----------------------------------------------------------------------------

DbDatum Database::get_device_family(string &wildcard)
{
	Any send;
	send <<= wildcard.c_str();

	Any_var received = command_inout("DbGetDeviceFamilyList",send);
	const DevVarStringArray *device_family;
	received >>= device_family;

	DbDatum db_datum;
	int n_familys;
	n_familys = device_family->length();
	db_datum.name = wildcard;
	db_datum.value_string.resize(n_familys);
	for (int i=0; i<n_familys; i++)
	{
		db_datum.value_string[i] = (*device_family)[i]; 
	}
		
	return db_datum;
}


//-----------------------------------------------------------------------------
//
// Database::get_device_domain() - public method to get device domains from
//                               the Database
//
//-----------------------------------------------------------------------------

DbDatum Database::get_device_domain(string &wildcard)
{
	Any send;
	send <<= wildcard.c_str();

	Any_var received = command_inout("DbGetDeviceFamilyList",send);
	const DevVarStringArray *device_domain;
	received >>= device_domain;

	DbDatum db_datum;
	int n_domains;
	n_domains = device_domain->length();
	db_datum.name = wildcard;
	db_datum.value_string.resize(n_domains);
	for (int i=0; i<n_domains; i++)
	{
		db_datum.value_string[i] = (*device_domain)[i]; 
	}
		
	return db_datum;
}

//-----------------------------------------------------------------------------
//
// Database::get_property() - public method to get object property from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::get_property(string obj, DbData &db_data)
{
	unsigned int i;
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(obj.c_str());
	for (i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbGetProperty",send);
	const DevVarStringArray *property_values;
	received >>= property_values;

	int n_props, index;
#ifdef STRSTREAM
	strstream iostream; 
#else
	stringstream iostream;
#endif /* STRSTREAM */
	iostream << (*property_values)[1].in();
	iostream >> n_props;
	index = 2;
	for (i=0; i<n_props; i++)
	{
		int n_values;
		db_data[i].name = (*property_values)[index]; index++;	
		iostream.clear(); iostream.seekp (0);  iostream.seekg (0);
		iostream << (*property_values)[index].in() << ends; 
		iostream >> n_values;
		index++;
	
		db_data[i].value_string.resize(n_values);
		if (n_values == 0)
		{
			index++; // skip dummy returned value " "
		}
		else
		{
			for (int j=0; j<n_values; j++)
			{
				db_data[i].value_string[j] = (*property_values)[index]; 
				index++;
			}
		}
	}
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::put_property() - public method to put object property from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::put_property(string obj, DbData &db_data)
{
#ifdef STRSTREAM
	ostrstream ostream;
#else
	ostringstream ostream;
#endif /* STRSTREAM */
	Any send;
	int index;
	
	DevVarStringArray *property_values = new DevVarStringArray;
	property_values->length(2); index = 2;
	(*property_values)[0] = string_dup(obj.c_str());
	ostream << db_data.size();
#ifdef STRSTREAM
	ostream << ends;
	(*property_values)[1] = string_dup(ostream.str());
	ostream.rdbuf()->freeze(false);
#else
	string st = ostream.str();
	(*property_values)[1] = string_dup(st.c_str());
#endif /* STRSTREAM */
	for (unsigned int i=0; i<db_data.size(); i++)
	{
		index++; property_values->length(index);
		(*property_values)[index-1] = string_dup(db_data[i].name.c_str());
		ostream.clear(); ostream.seekp (0);
		ostream << db_data[i].size() << ends;
		index++; property_values->length(index);
#ifdef STRSTREAM
		ostream << ends;
		(*property_values)[index-1] = string_dup(ostream.str());
		ostream.rdbuf()->freeze(false);
#else
		string st = ostream.str();
		(*property_values)[index-1] = string_dup(st.c_str());
#endif /* STRSTREAM */
		for (int j=0; j<db_data[i].size(); j++)
		{
			index++; property_values->length(index);
			(*property_values)[index-1] = string_dup(db_data[i].value_string[j].c_str());
		}
	}
	send <<= property_values;

	Any_var received = command_inout("DbPutProperty",send);
		
	return;
}

//-----------------------------------------------------------------------------
//
// Database::delete_property() - public method to delete object properties from
//                                   the Database
//
//-----------------------------------------------------------------------------

void Database::delete_property(string obj, DbData &db_data)
{
	Any send;
	DevVarStringArray *property_names = new DevVarStringArray;
	property_names->length(db_data.size()+1);
	(*property_names)[0] = string_dup(obj.c_str());
	for (unsigned int i=0; i<db_data.size(); i++)
	{
		(*property_names)[i+1] = string_dup(db_data[i].name.c_str());
	}
	send <<= property_names;

	Any_var received = command_inout("DbDeleteProperty",send);

	return;
}

} // End of Tango namespace
