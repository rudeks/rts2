#include "rts2nvaluebox.h"
#include "nmonitor.h"

Rts2NValueBox::Rts2NValueBox (Rts2NWindow * top, Rts2Value * in_val)
{
  topWindow = top;
  val = in_val;
}

Rts2NValueBox::~Rts2NValueBox (void)
{

}

Rts2NValueBoxBool::Rts2NValueBoxBool (Rts2NWindow * top,
				      Rts2ValueBool * in_val, int x, int y):
Rts2NValueBox (top, in_val),
Rts2NSelWindow (top->getX () + x, top->getY () + y, 10, 4)
{
  maxrow = 2;
  setLineOffset (0);
  if (!in_val->getValueBool ())
    setSelRow (1);
}

int
Rts2NValueBoxBool::injectKey (int key)
{
  switch (key)
    {
    case KEY_ENTER:
    case K_ENTER:
      return 0;
      break;
    }
  return Rts2NSelWindow::injectKey (key);
}

void
Rts2NValueBoxBool::draw ()
{
  Rts2NSelWindow::draw ();
  werase (getWriteWindow ());
  mvwprintw (getWriteWindow (), 0, 1, "true");
  mvwprintw (getWriteWindow (), 1, 1, "false");
  refresh ();
}

void
Rts2NValueBoxBool::sendValue (Rts2Conn * connection)
{
  if (!connection->getOtherDevClient ())
    return;
  connection->
    queCommand (new
		Rts2CommandChangeValue (connection->getOtherDevClient (),
					getValue ()->getName (), '=',
					getSelRow () == 0));
}

Rts2NValueBoxDouble::Rts2NValueBoxDouble (Rts2NWindow * top, Rts2ValueDouble * in_val, int x, int y):
Rts2NValueBox (top, in_val),
Rts2NWindow (top->getX () + x, top->getY () + y, 10, 3)
{
}

int
Rts2NValueBoxDouble::injectKey (int key)
{
  switch (key)
    {
    case KEY_LEFT:
      break;
    case KEY_RIGHT:
      break;
    case KEY_ENTER:
    case K_ENTER:
      return 0;
      break;
    }
  if (isdigit (key) || key == '.' || key == ',' || key == '+' || key == '-')
    {

    }
  return -1;
}

void
Rts2NValueBoxDouble::draw ()
{
  Rts2NWindow::draw ();
  refresh ();
}

void
Rts2NValueBoxDouble::sendValue (Rts2Conn * connection)
{
  if (!connection->getOtherDevClient ())
    return;
  connection->
    queCommand (new
		Rts2CommandChangeValue (connection->getOtherDevClient (),
					getValue ()->getName (), '=', 10));
}
