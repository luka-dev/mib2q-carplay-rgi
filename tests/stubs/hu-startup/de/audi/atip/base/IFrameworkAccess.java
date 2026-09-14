package de.audi.atip.base;
import de.audi.atip.hmi.IHMIServiceApp;
public interface IFrameworkAccess { long getMonotonicTime(); int getKombiType(); IHMIServiceApp getHmiServiceApp(); }
