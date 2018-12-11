#ifndef ACCOUNT_H_
#define ACCOUNT_H_
	
class account {
	public:
		string login;
		string password;
		int token;
		
	account(string c_login, string c_password, int c_token) {
		login = c_login;
		password = c_password;
		token = c_token;
	}
	
	account(string c_login, string c_password) {
		login = c_login;
		password = c_password;
	}	
};

#endif /* ACCOUNT_H_ */
