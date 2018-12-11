#include "config.hpp"

int main() {
	try{
		yaServer yaServer;
		thread thread(bind(&yaServer::messages_process, &yaServer));
		yaServer.run();
		thread.join();
	} catch (websocketpp::exception const & e) {
		std::cout << "Error in main: " << e.what() << std::endl;
	}
	return 0;
}
