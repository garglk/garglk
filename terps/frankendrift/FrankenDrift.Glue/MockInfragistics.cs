using System.Collections.Generic;

namespace FrankenDrift.Glue.Infragistics.Win.UltraWinToolbars
{
    public interface UltraToolbarsManager
    {
        public List<UltraToolbar> Tools { get; set; }
    }

    public interface UltraToolbar
    {
        public SharedProperties SharedProps { get; set; }
    }

    public interface SharedProperties
    {
        public string Tag { get; set; }
    }
}
