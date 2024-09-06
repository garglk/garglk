using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FrankenDrift.Glue
{
    public struct Point2D
    {
        public int X;
        public int Y;

        public Point2D(int x, int y) { X = x; Y = y; }
    }

    public enum ConceptualDashStyle
    {
        Dot = 1,
        Solid = 2
    }

    public interface Map
    {
        public void RecalculateNode(object node);
        public void SelectNode(string key);
    }
}
