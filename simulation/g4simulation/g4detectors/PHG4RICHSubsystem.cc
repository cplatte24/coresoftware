// $$Id: PHG4RICHSubsystem.cc,v 1.1 2013/10/01 00:33:01 jinhuang Exp $$

/*!
 * \file PHG4RICHSubsystem.cc
 * \brief
 * \author Jin Huang <jhuang@bnl.gov>
 * \version $$Revision: 1.1 $$
 * \date $$Date: 2013/10/01 00:33:01 $$
 */

#include "PHG4RICHSubsystem.h"
#include "PHG4RICHDetector.h"
#include "PHG4RICHDisplayAction.h"
#include "PHG4RICHSteppingAction.h"

#include <g4main/PHG4HitContainer.h>

#include <phool/getClass.h>

using namespace ePHENIXRICH;
using namespace std;

//_______________________________________________________________________
PHG4RICHSubsystem::PHG4RICHSubsystem(const string& name)
  : PHG4Subsystem(name)
  , m_Detector(nullptr)
  , m_DisplayAction(nullptr)
{
}

//_______________________________________________________________________
PHG4RICHSubsystem::~PHG4RICHSubsystem()
{
  delete m_DisplayAction;
}

//_______________________________________________________________________
int PHG4RICHSubsystem::Init(PHCompositeNode* topNode)
{
  // create hit list
  PHG4HitContainer* rich_hits = findNode::getClass<PHG4HitContainer>(topNode, "G4HIT_RICH");
  if (!rich_hits)
  {
    PHNodeIterator iter(topNode);
    PHCompositeNode* dstNode = dynamic_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "DST"));
    dstNode->addNode(new PHIODataNode<PHObject>(new PHG4HitContainer("G4HIT_RICH"), "G4HIT_RICH", "PHObject"));
  }

  // create display settings before detector
  m_DisplayAction = new PHG4RICHDisplayAction(Name());
  // create detector
  m_Detector = new PHG4RICHDetector(this, topNode, geom);
  m_Detector->Verbosity(Verbosity());
  m_Detector->OverlapCheck(CheckOverlap());

  // create stepping action

  return 0;
}

//_______________________________________________________________________
int PHG4RICHSubsystem::process_event(PHCompositeNode* topNode)
{
  if (PHG4RICHSteppingAction* p = dynamic_cast<PHG4RICHSteppingAction*>(m_Detector->GetSteppingAction()))
    p->SetInterfacePointers(topNode);

  return 0;
}

//_______________________________________________________________________
PHG4Detector* PHG4RICHSubsystem::GetDetector(void) const
{
  return m_Detector;
}
