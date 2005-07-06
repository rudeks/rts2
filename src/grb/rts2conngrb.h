#ifndef __RTS2_GRBCONN__
#define __RTS2_GRBCONN__

#include "../utils/rts2block.h"
#include "../utils/rts2device.h"
#include "../utils/rts2client.h"

#include "grbconst.h"

class Rts2ConnGrb:public Rts2Conn
{
private:
  long lbuf[SIZ_PKT];		// local buffer - swaped for Linux 
  long nbuf[SIZ_PKT];		// network buffer
  time_t last_packet;
  long here_sod;		// machine SOD (seconds after 0 GMT)
  double last_imalive_sod;	// SOD of the previous imalive packet

  // utility functions..
  double getPktSod ();

  double getJDfromTJD (long TJD, double SOD)
  {
    return TJD + 2440000.5 + SOD;
  }
  // process various messages..
  int pr_imalive ();
  int pr_swift_point ();	// swift pointing.

  // GRB db stuff
  int addSwiftPoint (double ra, double dec, double roll, const time_t * t,
		     char *name, float obstime, float merit);

  int gcn_port;
  char *gcn_hostname;
public:
  Rts2ConnGrb (char *in_gcn_hostname, int in_gcn_port,
	       Rts2Device * in_master);
  virtual ~ Rts2ConnGrb (void);
  virtual int idle ();
  virtual int init ();
  virtual int receive (fd_set * set);
};

#endif /* !__RTS2_GRBCONN__ */
