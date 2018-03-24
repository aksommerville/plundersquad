/* ps_mshid.h
 * Interface to Microsoft Windows "raw input" HID provider.
 * Raw input events are delivered via the main event loop.
 * So we depend on 'mswm' unit to deliver events to us, and our 'update' is a no-op.
 */

#ifndef PS_MSHID_H
#define PS_MSHID_H

int ps_mshid_init();
void ps_mshid_quit();

/* mswm calls this on WM_INPUT events.
 * We do nothing and report success if mshid is not initialized.
 */
int ps_mshid_event(int wparam,int lparam);

#endif
