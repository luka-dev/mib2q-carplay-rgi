package de.audi.atip.hmi.view;
public interface Screen {
    int getID();
    boolean isPartialPopupBlocked(IPartialPopupController popup);
}
