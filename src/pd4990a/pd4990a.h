/*
 *	Header file for the PD4990A Serial I/O calendar & clock.
 */

struct pd4990a_s
{
  int seconds;
  int minutes;
  int hours;
  int days;
  int month;
  int year;
  int weekday;
};

extern struct pd4990a_s pd4990a;

#define READ8_HANDLER(name) 	unsigned char  name(unsigned int offset)
#define WRITE8_HANDLER(name) 	void   name(unsigned int offset, unsigned char data)
#define WRITE16_HANDLER(name)	void   name(unsigned int offset, unsigned short data, unsigned short mem_mask)

void pd4990a_addretrace (void);
READ8_HANDLER (pd4990a_testbit_r);
READ8_HANDLER (pd4990a_databit_r);
WRITE8_HANDLER (pd4990a_control_w);
WRITE16_HANDLER (pd4990a_control_16_w);
void pd4990a_increment_day (void);
void pd4990a_increment_month (void);
