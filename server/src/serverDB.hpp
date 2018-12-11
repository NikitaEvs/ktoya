#ifndef SERVERDB_H_
#define SERVERDB_H_


/* Connection pool for pinebase class */
class pinepool {
	public:
		void createPool(int currentCount, string currentConnectionCommand) {
			countOfConnections = currentCount;
			connectionCommand = currentConnectionCommand;
			std::lock_guard<std::mutex> locker(mutex);
			/* Add new connections in pool */
			for(int i = 0; i < countOfConnections; i++) {
				pool.emplace(std::make_shared<pqxx::connection>(connectionCommand)); 
			}
		}
		
		/* Get free connection from pool */
		std::shared_ptr<pqxx::connection> getConnection() {
			std::unique_lock<std::mutex> lock(mutex);
			/* Wait for free connection */
			while(pool.empty()) {
				condition.wait(lock);
			}	
			std::shared_ptr<pqxx::connection> connection = pool.front();
			pool.pop();
			return connection;
		}
		
		/* Return connection in pool */
		void returnConnection(std::shared_ptr<pqxx::connection> connection) {
			std::unique_lock<std::mutex> lock(mutex);
			pool.push(connection);
			lock.unlock();
			condition.notify_one();
		}
		
		void closeAllConnections() {
			while(!pool.empty()){
				auto conn = pool.front();
				conn.get() -> disconnect();
				pool.pop();
			}
		}
	private:
		int countOfConnections;
		string connectionCommand;
		
		std::queue<std::shared_ptr<pqxx::connection> > pool;
		std::mutex mutex;
		std::condition_variable condition;
};

string SELECT_WITH_NAME = "SELECT * FROM ktoya WHERE login LIKE \'";

class serverDB {
	public:
		/* Read and parse configure file for database, use it in advance */
		void readConfigureFile() {
			/* Open configure file */
			string dbParamStr;
			std::ifstream configFile("config/database.json", std::ios::binary);
			configFile.seekg(0, std::ios_base::end);
			dbParamStr.resize(configFile.tellg());
			configFile.seekg(0, std::ios_base::beg);
			configFile.read((char*)dbParamStr.data(), (int)dbParamStr.size());
			json dbParam = json::parse(dbParamStr);
			dbname = dbParam["param"]["dbname"];
			user = dbParam["param"]["user"];
			password = dbParam["param"]["password"];
			hostaddr = dbParam["param"]["hostaddr"];
			port = dbParam["param"]["port"];
			configFile.close();
		}
			
		/* Return connection command for database */
		string getConnectionCommand() {
			return "dbname = " + dbname + " user = " + user + " password = " + password + " hostaddr = " + hostaddr + " port = " + port;
		}
		
		/* Simple initialization for read configure file, not working for anything else! */
		serverDB() {
			readConfigureFile();
		}
		
		/* Main initialization */
		serverDB(std::shared_ptr<pinepool> currentPinepoolPtr) {
			pinepoolPtr = currentPinepoolPtr;
		}
			
		/* Execute SELECT operation and return result */
		pqxx::result read(string command) {
			auto connection = pinepoolPtr -> getConnection();
			
			pqxx::nontransaction non(*connection);
			pqxx::result res(non.exec(command));
			
			pinepoolPtr -> returnConnection(connection);
			return res;
		}
	
		account getAccount(string login) {
			string command = SELECT_WITH_NAME + login + "';";
			pqxx::result res = read(command);
			if (res.size() == 0) {
				account accountTemp("", "", -1);
				return accountTemp;
			} else {
				for(pqxx::result::const_iterator c = res.begin(); c != res.end(); ++c) {
					account accountTemp(c[0].as<string>(), c[1].as<string>(), c[2].as<int>());
					return accountTemp;
				}
			}
		}
		
		account getServerViaRegData(string login, string pass) {
			account accountTemp = getAccount(login);
			if(accountTemp.token == -1) {
				return accountTemp;
			}
			if(accountTemp.password != pass) {
				account accountRes("", "", -1);
				return accountRes;
			}
			return accountTemp;
		}
	
	private:
		string dbname, user, password, hostaddr, port;
		std::shared_ptr<pinepool> pinepoolPtr;
};

#endif /* PINEDB_H_ */
