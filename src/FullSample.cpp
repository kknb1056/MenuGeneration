asdfa#include "l1menu/FullSample.h"

#include <stdexcept>
#include <cmath>

#include <TSystem.h>
#include "FWCore/FWLite/interface/AutoLibraryLoader.h"

#include "l1menu/L1TriggerDPGEvent.h"
#include "l1menu/ICachedTrigger.h"
#include "l1menu/IMenuRate.h"
#include "./implementation/MenuRateImplementation.h"
#include "L1UpgradeNtuple.h"
#include "UserCode/L1TriggerUpgrade/interface/L1AnalysisDataFormat.h"
#include "UserCode/L1TriggerUpgrade/interface/L1AnalysisL1ExtraUpgradeDataFormat.h"
#include "UserCode/L1TriggerDPG/interface/L1AnalysisEventDataFormat.h"
#include "UserCode/L1TriggerDPG/interface/L1AnalysisGTDataFormat.h"
#include "UserCode/L1TriggerDPG/interface/L1AnalysisGMTDataFormat.h"

namespace // Use the unnamed namespace for things only used in this file
{
	/** @brief A required implementation that just acts as a proxy.
	 *
	 * For some implementations of ISample, this class can speed up execution significantly. This
	 * implementation has no improvement. An implementation is required however, so this just acts
	 * as a proxy to the full routines.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 02/Jul/2013
	 */
	class CachedTriggerImplementation : public l1menu::ICachedTrigger
	{
	public:
		CachedTriggerImplementation( const l1menu::ITrigger& trigger ) : trigger_(trigger) {}
		virtual bool apply( const l1menu::IEvent& event ) { return event.passesTrigger( trigger_ ); }
	protected:
		const l1menu::ITrigger& trigger_;
	}; // end of class CachedTriggerImplementation
} // end of the unnamed namespace

namespace l1menu
{
	class FullSamplePrivateMembers
	{
	private:
		static const size_t PHIBINS;
		static const double PHIBIN[];
		static const size_t ETABINS;
		static const double ETABIN[];

		static bool libraryLoaderInitiated; ///< @brief Flag to say if libFWCoreFWLite.so has been loaded and the AutoLibraryLoader enabled

		double degree( double radian );
		int phiINjetCoord( double phi );
		int etaINjetCoord( double eta );
		double calculateHTT( const L1Analysis::L1AnalysisDataFormat& event );
		double calculateHTM( const L1Analysis::L1AnalysisDataFormat& event );
	public:
		FullSamplePrivateMembers( FullSample* pThisObject );
		void fillDataStructure( int selectDataInput );
		void fillL1Bits();
		L1UpgradeNtuple inputNtuple;
		l1menu::L1TriggerDPGEvent currentEvent;
		float sumOfWeights;
		float eventRate;
	};
}

const size_t l1menu::FullSamplePrivateMembers::PHIBINS=18;
const double l1menu::FullSamplePrivateMembers::PHIBIN[]={10,30,50,70,90,110,130,150,170,190,210,230,250,270,290,310,330,350};
const size_t l1menu::FullSamplePrivateMembers::ETABINS=23;
const double l1menu::FullSamplePrivateMembers::ETABIN[]={-5.,-4.5,-4.,-3.5,-3.,-2.172,-1.74,-1.392,-1.044,-0.696,-0.348,0,0.348,0.696,1.044,1.392,1.74,2.172,3.,3.5,4.,4.5,5.};
bool l1menu::FullSamplePrivateMembers::libraryLoaderInitiated=false;

l1menu::FullSamplePrivateMembers::FullSamplePrivateMembers( FullSample* pThisObject )
	: currentEvent(*pThisObject), sumOfWeights(-1), eventRate(1)
{
	if( !libraryLoaderInitiated )
	{
		gSystem->Load("libFWCoreFWLite.so");
		AutoLibraryLoader::enable();
		libraryLoaderInitiated=true;
	}
}

double l1menu::FullSamplePrivateMembers::degree( double radian )
{
	if( radian<0 ) return 360.+(radian/M_PI*180.);
	else return radian/M_PI*180.;
}

int l1menu::FullSamplePrivateMembers::phiINjetCoord( double phi )
{
	size_t phiIdx=0;
	double phidegree=degree( phi );
	for( size_t idx=0; idx<PHIBINS; idx++ )
	{
		if( phidegree>=PHIBIN[idx] and phidegree<PHIBIN[idx+1] ) phiIdx=idx;
		else if( phidegree>=PHIBIN[PHIBINS-1] || phidegree<=PHIBIN[0] ) phiIdx=idx;
	}
	phiIdx=phiIdx+1;
	if( phiIdx==18 ) phiIdx=0;
	return int( phiIdx );
}

int l1menu::FullSamplePrivateMembers::etaINjetCoord( double eta )
{
	size_t etaIdx=0.;
	for( size_t idx=0; idx<ETABINS; idx++ )
	{
		if( eta>=ETABIN[idx] and eta<ETABIN[idx+1] ) etaIdx=idx;
	}
	return int( etaIdx );
}

double l1menu::FullSamplePrivateMembers::calculateHTT( const L1Analysis::L1AnalysisDataFormat& event )
{
	double httValue=0.;

	// Calculate our own HT and HTM from the jets that survive the double jet removal.
	for( int i=0; i<event.Njet; i++ )
	{
		if( event.Bxjet.at( i )==0 && !event.Taujet.at(i) )
		{
			if( event.Etajet.at( i )>4 and event.Etajet.at( i )<17 )
			{
				httValue+=event.Etjet.at( i );
			} //in proper eta range
		} //correct beam crossing
	} //loop over cleaned jets

	return httValue;
}

double l1menu::FullSamplePrivateMembers::calculateHTM( const L1Analysis::L1AnalysisDataFormat& event )
{
	double htmValue=0.;
	double htmValueX=0.;
	double htmValueY=0.;

	// Calculate our own HT and HTM from the jets that survive the double jet removal.
	for( int i=0; i<event.Njet; i++ )
	{
		if( event.Bxjet.at( i )==0 && !event.Taujet.at(i) )
		{
			if( event.Etajet.at( i )>4 and event.Etajet.at( i )<17 )
			{

				//  Get the phi angle  towers are 0-17 (this is probably not real mapping but OK for just magnitude of HTM
				float phi=2*M_PI*(event.Phijet.at( i )/18.);
				htmValueX+=cos( phi )*event.Etjet.at( i );
				htmValueY+=sin( phi )*event.Etjet.at( i );

			} //in proper eta range
		} //correct beam crossing
	} //loop over cleaned jets

	htmValue=sqrt( htmValueX*htmValueX+htmValueY*htmValueY );

	return htmValue;
}

void l1menu::FullSamplePrivateMembers::fillDataStructure( int selectDataInput )
{
	// Use a reference for ease of use
	L1Analysis::L1AnalysisDataFormat& analysisDataFormat=currentEvent.rawEvent();

	analysisDataFormat.Reset();

	// Grab standard event information
	currentEvent.setWeight( inputNtuple.event_->puWeight );
	analysisDataFormat.Run=inputNtuple.event_->run;
	analysisDataFormat.LS=inputNtuple.event_->lumi;
	analysisDataFormat.Event=inputNtuple.event_->event;

	/* =======================================================================================================
	 /    Select the input source information
	 / ---------------------------------------------------------------------------------------------------------
	 /    case  0: Use Original L1ExtraTree that came with the event
	 /    case  1: Use reEmulated L1ExtraTree that was produced (note this may not be present or may be identical to the Original tree)
	 /    case  2: Use Original L1ExtraTree that came with the event except for muons which get from GMT. (For old Ntuple with no quality flag in L1Extra)
	 /
	 /	  case 10: Use Original L1Tree (GMT/GT) that came with the event
	 /    case 11: Use reEmulated L1Tree (GMT/GT) that was produced (note this may not be present or may be identical to the Original tree)
	 /    case 12: Use Original L1Tree (GMT/GCT) that was produced
	 /    case 13: Use reEmulated L1Tree (GMT/GCT) that was produced (note this may not be present or may be identical to the Original tree)
	 /
	 /    case 21: Use the L1ExtraUpgradeTree (Assuming Stage 1 Quantities filled in tree)
	 /    case 22: Use the L1ExtraUpgradeTree (Assuming Stage 2 Quantities filled in tree)
	 / ======================================================================================================= */
	switch( selectDataInput )
	{
		case 22:  //Select from L1ExtraUpgradeTree (Stage 2)

			// NOTES:  Stage 1 has EG Relaxed and EG Isolated.  The isolated EG are a subset of the Relaxed.
			//         so sort through the relaxed list and flag those that also appear in the isolated list.
			for( unsigned int i=0; i<inputNtuple.l1upgrade_->nEG; i++ )
			{

				analysisDataFormat.Bxel.push_back( inputNtuple.l1upgrade_->egBx.at( i ) );
				analysisDataFormat.Etel.push_back( inputNtuple.l1upgrade_->egEt.at( i ) );
				analysisDataFormat.Phiel.push_back( phiINjetCoord( inputNtuple.l1upgrade_->egPhi.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with phiINjetCoord
				analysisDataFormat.Etael.push_back( etaINjetCoord( inputNtuple.l1upgrade_->egEta.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with etaINjetCoord

				// Check whether this EG is located in the isolation list
				bool isolated=false;
				bool fnd=false;
				unsigned int isoEG=0;
				while( !fnd && isoEG < inputNtuple.l1upgrade_->nIsoEG )
				{
					if( inputNtuple.l1upgrade_->isoEGPhi.at( isoEG )==inputNtuple.l1upgrade_->egPhi.at( i )
							&& inputNtuple.l1upgrade_->isoEGEta.at( isoEG )==inputNtuple.l1upgrade_->egEta.at( i ) )
					{
						isolated=true;
						fnd=true;
					}
					isoEG++;
				}
				analysisDataFormat.Isoel.push_back( isolated );
				analysisDataFormat.Nele++;
			}

			// Note:  Taus are in the jet list.  Decide what to do with them. For now
			//  leave them the there as jets (not even flagged..)
			for( unsigned int i=0; i<inputNtuple.l1upgrade_->nJets; i++ )
			{

				// For each jet look for a possible duplicate if so remove it.
				bool duplicate=false;
				for( unsigned int j=0; j<i; j++ )
				{
					if( inputNtuple.l1upgrade_->jetBx.at( i )==inputNtuple.l1upgrade_->jetBx.at( j )
							&& inputNtuple.l1upgrade_->jetEt.at( i )==inputNtuple.l1upgrade_->jetEt.at( j )
							&& inputNtuple.l1upgrade_->jetEta.at( i )==inputNtuple.l1upgrade_->jetEta.at( j )
							&& inputNtuple.l1upgrade_->jetPhi.at( i )==inputNtuple.l1upgrade_->jetPhi.at( j ) )
					{
						duplicate=true;
						//printf("Duplicate jet found and removed \n");
					}
				}

				if( !duplicate )
				{
					analysisDataFormat.Bxjet.push_back( inputNtuple.l1upgrade_->jetBx.at( i ) );
					analysisDataFormat.Etjet.push_back( inputNtuple.l1upgrade_->jetEt.at( i ) );
					analysisDataFormat.Phijet.push_back( phiINjetCoord( inputNtuple.l1upgrade_->jetPhi.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with phiINjetCoord
					analysisDataFormat.Etajet.push_back( etaINjetCoord( inputNtuple.l1upgrade_->jetEta.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with etaINjetCoord
					analysisDataFormat.Taujet.push_back( false );
					analysisDataFormat.isoTaujet.push_back( false );
					//analysisDataFormat.Fwdjet.push_back(false); //COMMENT OUT IF JET ETA FIX

					//if(fabs(inputNtuple.l1upgrade_->jetEta.at(i))>=3.0) printf("Et %f  Eta  %f  iEta  %f Phi %f  iPhi  %f \n",analysisDataFormat.Etjet.at(analysisDataFormat.Njet),inputNtuple.l1upgrade_->jetEta.at(i),analysisDataFormat.Etajet.at(analysisDataFormat.Njet),inputNtuple.l1upgrade_->jetPhi.at(i),analysisDataFormat.Phijet.at(analysisDataFormat.Njet));
					//  Eta Jet Fix.  Some Jets with eta>3 has appeared in central jet list.  Move them by hand
					//  This is a problem in Stage 2 Jet code.
					(fabs( inputNtuple.l1upgrade_->jetEta.at( i ) )>=3.0) ? analysisDataFormat.Fwdjet.push_back( true ) : analysisDataFormat.Fwdjet.push_back( false );

					analysisDataFormat.Njet++;
				}
			}

			for( unsigned int i=0; i<inputNtuple.l1upgrade_->nFwdJets; i++ )
			{

				analysisDataFormat.Bxjet.push_back( inputNtuple.l1upgrade_->fwdJetBx.at( i ) );
				analysisDataFormat.Etjet.push_back( inputNtuple.l1upgrade_->fwdJetEt.at( i ) );
				analysisDataFormat.Phijet.push_back( phiINjetCoord( inputNtuple.l1upgrade_->fwdJetPhi.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with phiINjetCoord
				analysisDataFormat.Etajet.push_back( etaINjetCoord( inputNtuple.l1upgrade_->fwdJetEta.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with etaINjetCoord
				analysisDataFormat.Taujet.push_back( false );
				analysisDataFormat.isoTaujet.push_back( false );
				analysisDataFormat.Fwdjet.push_back( true );

				analysisDataFormat.Njet++;
			}

			// NOTES:  Stage 1 has Tau Relaxed and TauIsolated.  The isolated Tau are a subset of the Relaxed.
			//         so sort through the relaxed list and flag those that also appear in the isolated list.

			for( unsigned int i=0; i<inputNtuple.l1upgrade_->nTau; i++ )
			{

				// remove duplicates
				bool duplicate=false;
				for( unsigned int j=0; j<i; j++ )
				{
					if( inputNtuple.l1upgrade_->tauBx.at( i )==inputNtuple.l1upgrade_->tauBx.at( j )
							&& inputNtuple.l1upgrade_->tauEt.at( i )==inputNtuple.l1upgrade_->tauEt.at( j )
							&& inputNtuple.l1upgrade_->tauEta.at( i )==inputNtuple.l1upgrade_->tauEta.at( j )
							&& inputNtuple.l1upgrade_->tauPhi.at( i )==inputNtuple.l1upgrade_->tauPhi.at( j ) )
					{
						duplicate=true;
						//printf("Duplicate jet found and removed \n");
					}
				}

				if( !duplicate )
				{
					analysisDataFormat.Bxjet.push_back( inputNtuple.l1upgrade_->tauBx.at( i ) );
					analysisDataFormat.Etjet.push_back( inputNtuple.l1upgrade_->tauEt.at( i ) );
					analysisDataFormat.Phijet.push_back( phiINjetCoord( inputNtuple.l1upgrade_->tauPhi.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with phiINjetCoord
					analysisDataFormat.Etajet.push_back( etaINjetCoord( inputNtuple.l1upgrade_->tauEta.at( i ) ) ); //PROBLEM: real value, trigger wants bin convert with etaINjetCoord
					analysisDataFormat.Taujet.push_back( true );
					analysisDataFormat.Fwdjet.push_back( false );

					bool isolated=false;
					bool fnd=false;
					unsigned int isoTau=0;
					while( !fnd && isoTau < inputNtuple.l1upgrade_->nIsoTau )
					{
						if( inputNtuple.l1upgrade_->isoTauPhi.at( isoTau )==inputNtuple.l1upgrade_->tauPhi.at( i )
								&& inputNtuple.l1upgrade_->isoTauEta.at( isoTau )==inputNtuple.l1upgrade_->tauEta.at( i ) )
						{
							isolated=true;
							fnd=true;
						}
						isoTau++;
					}
					analysisDataFormat.isoTaujet.push_back( isolated );

					analysisDataFormat.Njet++;
				} // duplicate check
			}

			// Fill energy sums  (Are overflow flags accessible in l1extra?)
			for( unsigned int i=0; i<inputNtuple.l1upgrade_->nMet; i++ )
			{
				//if(inputNtuple.l1upgrade_->metBx.at(i)==0) {
				analysisDataFormat.ETT=inputNtuple.l1upgrade_->et.at( i );
				analysisDataFormat.ETM=inputNtuple.l1upgrade_->met.at( i );
				analysisDataFormat.PhiETM=inputNtuple.l1upgrade_->metPhi.at( i );
			}
			analysisDataFormat.OvETT=0; //not available in l1extra
			analysisDataFormat.OvETM=0; //not available in l1extra

			for( unsigned int i=0; i<inputNtuple.l1upgrade_->nMht; i++ )
			{
				if( inputNtuple.l1upgrade_->mhtBx.at( i )==0 )
				{
					analysisDataFormat.HTT=calculateHTT( analysisDataFormat ); //inputNtuple.l1upgrade_->ht.at(i) ;
					analysisDataFormat.HTM=calculateHTM( analysisDataFormat ); //inputNtuple.l1upgrade_->mht.at(i) ;
					analysisDataFormat.PhiHTM=0.; //inputNtuple.l1upgrade_->mhtPhi.at(i) ;
				}
			}
			analysisDataFormat.OvHTM=0; //not available in l1extra
			analysisDataFormat.OvHTT=0; //not available in l1extra

			// Get the muon information  from reEmul GMT
			for( int i=0; i<inputNtuple.gmtEmu_->N; i++ )
			{

				analysisDataFormat.Bxmu.push_back( inputNtuple.gmtEmu_->CandBx[i] );
				analysisDataFormat.Ptmu.push_back( inputNtuple.gmtEmu_->Pt[i] );
				analysisDataFormat.Phimu.push_back( inputNtuple.gmtEmu_->Phi[i] );
				analysisDataFormat.Etamu.push_back( inputNtuple.gmtEmu_->Eta[i] );
				analysisDataFormat.Qualmu.push_back( inputNtuple.gmtEmu_->Qual[i] );
				analysisDataFormat.Isomu.push_back( false );
				analysisDataFormat.Nmu++;
			}

		break;

		default:
			throw std::runtime_error( "---Not a valid input source FULL STOP! " );

		break;
	}

	return;
}

void l1menu::FullSamplePrivateMembers::fillL1Bits()
{
	bool* PhysicsBits=currentEvent.physicsBits();

	// I really don't think this if statement is correct. Surely it
	// should be "if( inputNtuple.gt_ )"? - M. Grimes.
	if( !inputNtuple.gt_ )
	{
		for( Int_t ibit=0; ibit<128; ibit++ )
		{
			PhysicsBits[ibit]=0;
			if( ibit<64 )
			{
				PhysicsBits[ibit]=(inputNtuple.gt_->tw1[2]>>ibit)&1;
			}
			else
			{
				PhysicsBits[ibit]=(inputNtuple.gt_->tw2[2]>>(ibit-64))&1;
			}
		}
	}
	else
	{
		PhysicsBits[0]=1; //set zero bias on if no gt information
	}
}


l1menu::FullSample::FullSample()
	: pImple_( new FullSamplePrivateMembers( this ) )
{
	// No operation besides the initialiser list
}

l1menu::FullSample::~FullSample()
{
	delete pImple_;
}

l1menu::FullSample::FullSample( const l1menu::FullSample& otherFullSample )
	: pImple_( new FullSamplePrivateMembers(*otherFullSample.pImple_) )
{
	// No operation besides the initialiser list
}

l1menu::FullSample::FullSample( l1menu::FullSample&& otherFullSample ) noexcept
	: pImple_( otherFullSample.pImple_ )
{
	otherFullSample.pImple_=NULL;
}

l1menu::FullSample& l1menu::FullSample::operator=( const l1menu::FullSample& otherFullSample )
{
	*pImple_=*otherFullSample.pImple_;
	return *this;
}

l1menu::FullSample& l1menu::FullSample::operator=( l1menu::FullSample&& otherFullSample ) noexcept
{
	pImple_=otherFullSample.pImple_;
	otherFullSample.pImple_=NULL;
	return *this;
}

void l1menu::FullSample::loadFile( const std::string& filename )
{
	pImple_->sumOfWeights=-1;
	pImple_->inputNtuple.Open( filename );
}

void l1menu::FullSample::loadFilesFromList( const std::string& filenameOfList )
{
	pImple_->sumOfWeights=-1;
	pImple_->inputNtuple.OpenWithList( filenameOfList );
}

const l1menu::L1TriggerDPGEvent& l1menu::FullSample::getFullEvent( size_t eventNumber ) const
{
	// Make sure the event number requested is valid. Use static_cast to get rid
	// of the "comparison between signed and unsigned" compiler warning.
	if( eventNumber>static_cast<size_t>(pImple_->inputNtuple.GetEntries()) ) throw std::runtime_error( "Requested event number is out of range" );

	pImple_->inputNtuple.LoadTree(eventNumber);
	pImple_->inputNtuple.GetEntry(eventNumber);
	// This next call fills pImple_->currentEvent with the information in pImple_->inputNtuple
	pImple_->fillDataStructure( 22 );
	pImple_->fillL1Bits();

	return pImple_->currentEvent;
}

const std::vector< std::pair<std::string,unsigned int> > l1menu::FullSample::availableArchitectures() const
{
	std::vector< std::pair<std::string,unsigned int> > returnValue;
	returnValue.push_back( std::make_pair( "Fallback", 0 ) );
	return returnValue;
}

bool l1menu::FullSample::providesArchitecture( const std::string& architectureName ) const
{
	if( architectureName=="Fallback" ) return true;
	else return false;
}

bool l1menu::FullSample::providesArchitecture( const std::string& architectureName, unsigned int version ) const
{
	if( architectureName=="Fallback" && version==0 ) return true;
	else return false;
}

const l1menu::ITriggerArchitecture& l1menu::FullSample::getArchitecture( const std::string& architectureName ) const
{
	if( architectureName!="Fallback" ) throw std::runtime_error( "Unsupported trigger architecture" );
	throw std::runtime_error( "Not implemented yet" );
}

const l1menu::ITriggerArchitecture& l1menu::FullSample::getArchitecture( const std::string& architectureName, unsigned int version ) const
{
	if( architectureName!="Fallback" || version!=0 ) throw std::runtime_error( "Unsupported trigger architecture" );
	throw std::runtime_error( "Not implemented yet" );
}

size_t l1menu::FullSample::numberOfEvents() const
{
	return static_cast<size_t>( pImple_->inputNtuple.GetEntries() );
}

const l1menu::IEvent& l1menu::FullSample::getEvent( size_t eventNumber ) const
{
	// This returns a derived class so just delegate to that
	return getFullEvent( eventNumber );
}

std::unique_ptr<l1menu::ICachedTrigger> l1menu::FullSample::createCachedTrigger( const l1menu::ITrigger& trigger ) const
{
	return std::unique_ptr<l1menu::ICachedTrigger>( new CachedTriggerImplementation(trigger) );
}

float l1menu::FullSample::eventRate() const
{
	return pImple_->eventRate;
}

void l1menu::FullSample::setEventRate( float rate )
{
	pImple_->eventRate=rate;
}

float l1menu::FullSample::sumOfWeights() const
{
	if( pImple_->sumOfWeights==-1 )
	{
		pImple_->sumOfWeights=0;
		for( int eventNumber=0; eventNumber<pImple_->inputNtuple.GetEntries(); ++eventNumber )
		{
			pImple_->inputNtuple.LoadTree(eventNumber);
			pImple_->inputNtuple.GetEntry(eventNumber);
			pImple_->sumOfWeights+=pImple_->inputNtuple.event_->puWeight;
		}
	}

	return pImple_->sumOfWeights;
}

std::shared_ptr<const l1menu::IMenuRate> l1menu::FullSample::rate( const l1menu::TriggerMenu& menu ) const
{
	return std::shared_ptr<const l1menu::IMenuRate>( new l1menu::implementation::MenuRateImplementation( menu, *this ) );
}
