package de.esolutions.hmi.widgets.audi.base;
import de.audi.atip.hmi.view.Screen;
/** Only the physical HMI widget boundary is replaced in the host test. */
public class AbstractScreenWidget implements Screen {
    private final int id;
    private int smallStage = 1;
    public AbstractScreenWidget(int id) { this.id = id; }
    public int getID() { return id; }
    public boolean isPartialPopupBlocked(de.audi.atip.hmi.view.IPartialPopupController popup) { return false; }
    public int getSmallStageType() { return smallStage; }
    public void setSmallStageType(int value) { smallStage = value; }
}
