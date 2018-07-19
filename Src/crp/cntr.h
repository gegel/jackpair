
void make_pkt(unsigned char* m);  //encrypt data packet or make control packet with outgoing counter value
short check_pkt(unsigned char* m); //decrypt data packet or sync incoming counter by value from control packet

void setkey(unsigned int pin); //generate session keyparing for SPEKE protocol using pin as authenticator
unsigned char txkey(unsigned char* m); //output next part of our_key for transmiting
unsigned char rxkey(unsigned char* m, unsigned char* sb); //process received next part of their_key using soft bits (LLR)

short setrand(unsigned int res);  //add one LSB of res to entropy collector, returns 'success' flag
void getrand(unsigned char* m, short len); //output len bytes of PRNG number
unsigned char testcrp(void); //self-test of assembled crypto engine
unsigned char ike_ber(unsigned char* m, signed char* logout); //output test log of modem statistic in IKE mode
void work_ber(unsigned char* m, signed char* logout); //optionally output test log of modem statistic in WORK mode
