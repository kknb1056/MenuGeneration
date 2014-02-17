#ifndef l1menu_ITriggerEvent_h
#define l1menu_ITriggerEvent_h


namespace l1menu
{
	/** @brief Abstract interface to provide all the triggerable quantities in an event.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 14/Feb/2014
	 */
	class ITriggerEvent
	{
	public:
		class IJet
		{
		public:
			virtual ~IJet() {}
			virtual int bunchCrossing() const = 0;
			virtual bool isForwardJet() const = 0;
			virtual bool isTauJet() const = 0;
			virtual float eta() const = 0;
			virtual float transverseEnergy() const = 0;
		};
	public:
		virtual ~ITriggerEvent() {}
		virtual const std::vector<l1menu::ITriggerEvent::IJet*> jets() const = 0;
	};

} // end of namespace l1menu

#endif
