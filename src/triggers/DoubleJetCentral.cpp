#include "../implementation/RegisterTriggerMacro.h"
#include "l1menu/L1TriggerDPGEvent.h"
#include "l1menu/ITriggerEvent.h"

#include <stdexcept>
#include "UserCode/L1TriggerUpgrade/interface/L1AnalysisDataFormat.h"

#include "l1menu/ITrigger.h"

#include <string>
#include <vector>

namespace l1menu
{
	namespace triggers
	{
		/** @brief Base class for all versions of the DoubleJetCentral trigger.
		 *
		 * Note that this class is abstract because it doesn't implement the "version"
		 * and "apply" methods. That's left up to the implementations of the different
		 * versions.
		 *
		 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
		 * @date 02/Jun/2013
		 */
		class DoubleJetCentral : public l1menu::ITrigger
		{
		public:
			DoubleJetCentral();

			virtual const std::string name() const;
			virtual const std::vector<std::string> parameterNames() const;
			virtual float& parameter( const std::string& parameterName );
			virtual const float& parameter( const std::string& parameterName ) const;
		protected:
			float threshold1_;
			float threshold2_;
			float regionCut_;
		}; // end of the DoubleJetCentral base class

		/** @brief First version of the DoubleJetCentral trigger.
		 *
		 * @author probably Brian Winer
		 * @date sometime
		 */
		class DoubleJetCentral_v0 : public DoubleJetCentral
		{
		public:
			virtual unsigned int version() const;
			virtual bool apply( const l1menu::L1TriggerDPGEvent& event ) const;
			virtual bool apply( const l1menu::ITriggerEvent& event ) const;
			virtual bool thresholdsAreCorrelated() const;
		}; // end of version 0 class


		/* The REGISTER_TRIGGER macro will make sure that the given trigger is registered in the
		 * l1menu::TriggerTable when the program starts. I also want to provide some suggested binning
		 * however. The REGISTER_TRIGGER_AND_CUSTOMISE macro does exactly the same but lets me pass
		 * a pointer to a function that will be called directly after the trigger has been registered
		 * at program startup. The function takes no parameters and returns void. In this case I'm
		 * giving it a lambda function.
		 */
		REGISTER_TRIGGER_AND_CUSTOMISE( DoubleJetCentral_v0,
			[]() // Use a lambda function to customise rather than creating a named function that never gets used again.
			{
				l1menu::TriggerTable& triggerTable=l1menu::TriggerTable::instance();
				DoubleJetCentral_v0 tempTriggerInstance;
				triggerTable.registerSuggestedBinning( tempTriggerInstance.name(), "threshold1", 100, 0, 400 );
				triggerTable.registerSuggestedBinning( tempTriggerInstance.name(), "threshold2", 100, 0, 400 );
			} // End of customisation lambda function
		) // End of REGISTER_TRIGGER_AND_CUSTOMISE macro call


	} // end of namespace triggers

} // end of namespace l1menu


//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//---------------  Definitions below         ---------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------


bool l1menu::triggers::DoubleJetCentral_v0::apply( const l1menu::L1TriggerDPGEvent& event ) const
{
	const L1Analysis::L1AnalysisDataFormat& analysisDataFormat=event.rawEvent();
	const bool* PhysicsBits=event.physicsBits();

	bool raw=PhysicsBits[0]; // ZeroBias
	if( !raw ) return false;

	int n1=0;
	int n2=0;
	int Nj=analysisDataFormat.Njet;
	for( int ue=0; ue<Nj; ue++ )
	{
		int bx=analysisDataFormat.Bxjet[ue];
		if( bx!=0 ) continue;
		bool isFwdJet=analysisDataFormat.Fwdjet[ue];
		if( isFwdJet ) continue;
		bool isTauJet=analysisDataFormat.Taujet[ue];
		if( isTauJet ) continue;

		float eta = analysisDataFormat.Etajet[ue];
		if (eta < regionCut_ || eta > 21.-regionCut_) continue;

		float rank=analysisDataFormat.Etjet[ue];
		float pt=rank; //CorrectedL1JetPtByGCTregions(analysisDataFormat.Etajet[ue],rank*4.,theL1JetCorrection);
		if( pt>=threshold1_ ) n1++;
		if( pt>=threshold2_ ) n2++;
	}
	bool ok=(n1>=1&&n2>=2);

	return ok;
}

bool l1menu::triggers::DoubleJetCentral_v0::apply( const l1menu::ITriggerEvent& event ) const
{
	size_t numberPassingThreshold1=0;
	size_t numberPassingThreshold2=0;

	for( const auto& jet : event.jets() )
	{
		if( jet->bunchCrossing()!=0 ) continue;
		if( jet->isForwardJet() ) continue;
		if( jet->isTauJet() ) continue;

		float eta=jet->eta();
		if( eta < regionCut_ || eta > 21.-regionCut_ ) continue;

		float pT=jet->transverseEnergy();
		if( pT>=threshold1_ ) ++numberPassingThreshold1;
		if( pT>=threshold2_ ) ++numberPassingThreshold2;
	}

	return numberPassingThreshold1>=1 && numberPassingThreshold2>=2;
}

bool l1menu::triggers::DoubleJetCentral_v0::thresholdsAreCorrelated() const
{
	return false;
}

unsigned int l1menu::triggers::DoubleJetCentral_v0::version() const
{
	return 0;
}

l1menu::triggers::DoubleJetCentral::DoubleJetCentral()
	: threshold1_(20), threshold2_(20), regionCut_(4.5)
{
	// No operation other than the initialiser list
}

const std::string l1menu::triggers::DoubleJetCentral::name() const
{
	return "L1_DoubleJet";
}

const std::vector<std::string> l1menu::triggers::DoubleJetCentral::parameterNames() const
{
	std::vector<std::string> returnValue;
	returnValue.push_back("threshold1");
	returnValue.push_back("threshold2");
	returnValue.push_back("regionCut");
	return returnValue;
}

float& l1menu::triggers::DoubleJetCentral::parameter( const std::string& parameterName )
{
	if( parameterName=="threshold1" ) return threshold1_;
	else if( parameterName=="threshold2" ) return threshold2_;
	else if( parameterName=="regionCut" ) return regionCut_;
	else throw std::logic_error( "Not a valid parameter name" );
}

const float& l1menu::triggers::DoubleJetCentral::parameter( const std::string& parameterName ) const
{
	if( parameterName=="threshold1" ) return threshold1_;
	else if( parameterName=="threshold2" ) return threshold2_;
	else if( parameterName=="regionCut" ) return regionCut_;
	else throw std::logic_error( "Not a valid parameter name" );
}
