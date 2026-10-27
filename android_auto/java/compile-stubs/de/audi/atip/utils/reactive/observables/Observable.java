package de.audi.atip.utils.reactive.observables;

import de.audi.atip.log.LogChannel;
import de.audi.atip.utils.dispatching.IDispatcher;
import de.audi.atip.utils.generics.Consumer;

/** COMPILE-ONLY stand-in (see compile-stubs/README.txt): only the calls the stock
 * ExternalEventsListener makes. */
public interface Observable {
    Observable log(LogChannel channel, String tag);
    Observable debounce(int millis, IDispatcher dispatcher);
    Subscription redirectTo(Consumer consumer);
}
