#ifndef l1menu_ITriggerArchitecture_h
#define l1menu_ITriggerArchitecture_h

//
// Forward declarations
//
namespace l1menu
{
	class ITriggerEvent;
}

namespace l1menu
{
	/** @brief Abstract interface for a representation of a complete trigger system.
	 *
	 * The idea is that each concrete implementation of this interface will be able to
	 * provide the output corresponding to a full trigger system. So there will be one
	 * implementation for the current trigger, one for Stage 2, one for a system with
	 * a track trigger and so on.
	 *
	 * The abstract interface provides a method to get an ITriggerEvent, from which it
	 * is possible to obtain quantities such as MET or whatever. It is the
	 * ITriggerArchitecture implementation's responsibility to create the ITriggerEvent
	 * so that it provides the correct information. So for example the architecture with
	 * a track trigger will provide an ITriggerEvent where the isolation on the EGamma
	 * comes from tracks; whereas the Stage 2 architecture will provide an ITriggerEvent
	 * where it comes solely from the calorimeter information.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 14/Feb/2014
	 */
	class ITriggerArchitecture
	{
	public:
		virtual ~ITriggerArchitecture() {}
		virtual const std::string name() const = 0;
		virtual unsigned int version() const = 0;
		virtual const l1menu::ITriggerEvent& getEvent() const = 0;
	};

} // end of namespace l1menu

#endif
