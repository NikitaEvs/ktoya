#ifndef SERVER_H_
#define SERVER_H_

enum action_type {
	SUBSCRIBE,
	UNSUBSCRIBE,
	MESSAGE
};

struct action {
	action(action_type type, connection_hdl handler) : 
		type(type), 
		handler(handler) {}
	action(action_type type, connection_hdl handler, server::message_ptr message) :  
		type(type),
		handler(handler),
		msg(message) {}
	action_type type;
	websocketpp::connection_hdl handler;
	server::message_ptr msg;
};

class yaServer {
	public:
		/* Simple init server with handlers */
		yaServer() {
			configure();
			readConfigureFile();
		}
		
		/* Server initialization */
		void configure() {
			std::cout << "Server initialization..." << std::endl;
			currentServer.init_asio();
			std::cout << "Setting handlers..." << std::endl;
			currentServer.set_open_handler(bind(&yaServer::on_open, this, ::_1));
			currentServer.set_close_handler(bind(&yaServer::on_close, this, ::_1));
			currentServer.set_message_handler(bind(&yaServer::on_message, this, ::_1, ::_2));
			std::cout << "Setting conditions..." << std::endl;
			currentServer.set_reuse_addr(true); // For fixing problem "Address already in use" (not work :C)
			currentRelay.push_back(std::make_pair(1, false));
		}
		
		/* Read and parse configure file for database, use it in advance */
		void readConfigureFile() {
			/* Open configure file */
			string dbParamStr;
			std::ifstream configFile("config/server.json", std::ios::binary);
			configFile.seekg(0, std::ios_base::end);
			dbParamStr.resize(configFile.tellg());
			configFile.seekg(0, std::ios_base::beg);
			configFile.read((char*)dbParamStr.data(), (int)dbParamStr.size());
			json dbParam = json::parse(dbParamStr);
			port = dbParam["param"]["port"];
			configFile.close();
		}
		
		/* Init server run configuration */
		void run() {
			std::cout << "Configure running..." << std::endl;
			currentServer.listen(port);
			currentServer.start_accept();
			try{
				std::cout << "Running..." << std::endl;
				currentServer.run();
			} catch (websocketpp::exception const & e) {
				std::cout << "Oops. Err code in running: " << e.what() << std::endl;
			} catch (...) {
				std::cout << "Unknown exception :c " << std::endl;
			}
		}
		
		
		/* Function was called with open new connection */
		void on_open(connection_hdl handler) {
			{
				std::cout << "OPEN CONNECTION" << std::endl;
				lock_guard<mutex> guard(action_lock);
				actions.push(action(SUBSCRIBE, handler)); 
			}
			action_cond.notify_one();
		}

		/* Function was called with close connection */
		void on_close(connection_hdl handler) {
			{
				std::cout << "CLOSE CONNECTION" << std::endl;
				lock_guard<mutex> guard(action_lock);
				actions.push(action(UNSUBSCRIBE, handler));	
			}
			action_cond.notify_one();
		}

		/* Function was called with new message */
		void on_message(connection_hdl handler, server::message_ptr msg) {
			{
				std::cout << "RECEIVE MESSAGE" << std::endl;
				lock_guard<mutex> guard(action_lock);
				actions.push(action(MESSAGE, handler, msg));
			}
			action_cond.notify_one();
		}

		/* Message analyze in infinity loop (using in thread!) */
		void messages_process() {
			while(true){
				unique_lock<mutex> lock(action_lock);
				while(actions.empty()){
					std::cout << "Wait for connections..." << std::endl;
					action_cond.wait(lock);
				}
				std::cout << "TAKE ACTION" << std::endl;
				action action = actions.front();
				actions.pop();
				lock.unlock();
				if(action.type == SUBSCRIBE){ 
					std::cout << "NEW CONNECTION" << std::endl;
					lock_guard<mutex> guard(connection_lock);
					websocketpp::lib::error_code errCode;
					currentServer.send(action.handler, createAuthReq(), websocketpp::frame::opcode::text, errCode);
				} else if(action.type == UNSUBSCRIBE){
					std::cout << "CLOSE CONNECTION" << std::endl;
					lock_guard<mutex> guard(connection_lock);
				} else if(action.type == MESSAGE){
					std::cout << "NEW MESSAGE" << std::endl;
					lock_guard<mutex> guard(connection_lock);
					websocketpp::lib::error_code errCode;
					currentServer.send(action.handler, analyzeJSON(action.msg -> get_payload()), websocketpp::frame::opcode::text, errCode);
				}
			}
		}
		
		/* Analyze input msg and return response */
		string analyzeJSON(string jsonStr) {
			std::cout << "Received message: " << jsonStr << std::endl;
			json inMsg = json::parse(jsonStr);
			json outMsg;
			string event = inMsg["event"];
			if(event == "set") {
				currentRelay[0] = std::make_pair(1, inMsg["data"]["status"]);
				outMsg["event"] = "response";
				outMsg["data"]["response"] = "ok";
			} else if(event == "get") {
				outMsg["event"] = "status";
				outMsg["data"]["status"] = currentRelay[0].second;
			} else {
				outMsg["event"] = "error";
				outMsg["data"]["response"] = "unknown request";
			}
			return outMsg.dump();
		}
		
		/* Create simple auth request */
		string createAuthReq() {
			json j;
			j["event"] = "authNotify";
			j["data"] = "null";
		}

	private:
		server currentServer;
		mutex action_lock;
		mutex connection_lock;
		std::queue<action> actions;
		condition_variable action_cond;
		std::vector<std::pair<int, bool> > currentRelay;
		int port;
};



#endif /* SERVER_H_ */
