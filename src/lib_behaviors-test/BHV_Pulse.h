/************************************************************/
/*    NAME: Konstantinos Voskakis                                              */
/*    ORGN: MIT                                             */
/*    FILE: BHV_Pulse.h                                      */
/*    DATE:                                                 */
/************************************************************/

#ifndef Pulse_HEADER
#define Pulse_HEADER

#include <string>
#include "IvPBehavior.h"
#include "XYRangePulse.h"

class BHV_Pulse : public IvPBehavior {
public:
  BHV_Pulse(IvPDomain);
  ~BHV_Pulse() {};
  
  bool         setParam(std::string param, std::string value);
  void         onSetParamComplete();
  void         onCompleteState();
  void         onIdleState();
  void         onHelmStart();
  void         postConfigStatus();
  void         onRunToIdleState();
  void         onIdleToRunState();
  IvPFunction* onRunState();


protected: // Local Utility functions

  
protected: // Configuration parameters

protected: // State variables
double m_osx;
  double m_osy;
  double m_curr_time;
  double m_wpt_index;       // Tracks the last known waypoint index
  double m_pulse_time;      // The scheduled time to drop the next pulse

  // From your setParam function
  double m_pulse_range;     
  double m_pulse_duration; 
};

#define IVP_EXPORT_FUNCTION

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain) 
  {return new BHV_Pulse(domain);}
}
#endif
