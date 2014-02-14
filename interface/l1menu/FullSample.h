#ifndef l1menu_FullSample_h
#define l1menu_FullSample_h

#include <string>
#include <memory>
#include <vector>
#include "l1menu/ISample.h"

// Forward declarations
namespace l1menu
{
	class L1TriggerDPGEvent;
	class ITriggerArchitecture;
}


namespace l1menu
{
	/** @brief An implementation of the ISample interface using the standard L1 DPG ntuples.
	 *
	 * It's called a "full" sample because it's not the "reduced" sample. It loads up standard
	 * L1 DPG ntuples for use in menu studies. This is the most low-level or "original" way
	 * of working because it runs straight off the ntuples. The other implementations of
	 * ISample will use data that in some way originally came from a FullSample.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date sometime around May 2013
	 */
	class FullSample : public l1menu::ISample
	{
	public:
		FullSample();
		virtual ~FullSample();
		FullSample( const FullSample& otherFullSample );
		FullSample( FullSample&& otherFullSample ) noexcept;
		FullSample& operator=( const FullSample& otherFullSample );
		FullSample& operator=( FullSample&& otherFullSample ) noexcept;

		void loadFile( const std::string& filename );
		void loadFilesFromList( const std::string& filenameOfList );
		const l1menu::L1TriggerDPGEvent& getFullEvent( size_t eventNumber ) const;

		virtual const std::vector< std::pair<std::string,unsigned int> > availableArchitectures() const;
		virtual bool providesArchitecture( const std::string& architectureName ) const;
		virtual bool providesArchitecture( const std::string& architectureName, unsigned int version ) const;
		virtual const l1menu::ITriggerArchitecture& getArchitecture( const std::string& architectureName ) const;
		virtual const l1menu::ITriggerArchitecture& getArchitecture( const std::string& architectureName, unsigned int version ) const;

		virtual size_t numberOfEvents() const;
		virtual const l1menu::IEvent& getEvent( size_t eventNumber ) const;
		virtual std::unique_ptr<l1menu::ICachedTrigger> createCachedTrigger( const l1menu::ITrigger& trigger ) const;
		virtual float eventRate() const;
		virtual void setEventRate( float rate );
		virtual float sumOfWeights() const;
		virtual std::shared_ptr<const l1menu::IMenuRate> rate( const l1menu::TriggerMenu& menu ) const;
	private:
		class FullSamplePrivateMembers* pImple_;
	}; // end of class FullSample

} // end of namespace l1menu

#endif
