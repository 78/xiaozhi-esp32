using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;

namespace WpfModuloTool
{
    public partial class MainWindow : Window
    {
        // 定义编码规则
        private readonly Dictionary<string, UInt32> encodingRules = new Dictionary<string, UInt32>
        {
            { "0:1", 0 },
            { "0:2", 0 },
            { "0:4", 0 },
            { "0:8", 0 },
            { "0:10", 0x10 },
            { "0:20", 0x20 },
            { "0:40", 0x40 },
            { "0:80", 0x80 },
            { "1:1", 0x100 },
            { "1:2", 0x200 },
            { "1:4", 0x400 },
            { "1:8", 0x800 },
            { "1:10", 0x1000 },
            { "1:20", 0x2000 },
            { "1:40", 0x4000 },
            { "1:80", 0x8000 },
            { "2:1", 0x10000 },
            { "2:2", 0x20000 },
            { "2:4", 0x40000 },
            { "2:8", 0x80000 },
            { "2:10", 0x100000 },
            { "2:20", 0x200000 },
            { "2:40", 0x400000 },
            { "2:80", 0x800000 },
        };

        // 存储选中的线段
        private readonly List<string> selectedSegments = new List<string>();

        // 定义每个字符需要点亮的线段
        private readonly Dictionary<char, List<string>> charSegments = new Dictionary<char, List<string>>
        {
            // 数字 0 - 9
            {'0', new List<string> { "2:80", "2:40", "2:20", "2:1", "1:1", "0:10", "0:40", "0:80", "1:10", "2:10" } },
            {'1', new List<string> { "2:20", "1:1", "0:10" } },
            {'2', new List<string> { "2:40", "2:20", "1:1", "1:20", "1:40", "0:20", "0:40" } },
            {'3', new List<string> { "2:40", "2:20", "1:1", "1:20", "0:10", "0:40" } },
            {'4', new List<string> { "2:80", "2:10", "1:10", "1:20", "2:20", "1:1" } },
            {'5', new List<string> { "2:40", "2:80", "1:10", "1:40", "1:20", "0:10", "0:40" } },
            {'6', new List<string> { "2:40", "2:80", "1:10", "1:40", "1:20", "0:10", "0:40", "0:80" } },
            {'7', new List<string> { "2:40", "2:20", "1:1" } },
            {'8', new List<string> { "2:80", "2:40", "2:20", "2:1", "1:1", "0:10", "0:40", "0:80", "1:10", "2:10", "1:40", "1:20" } },
            {'9', new List<string> { "2:40", "2:20", "2:1", "1:1", "0:10", "0:40", "1:20", "1:40", "2:10", "2:80" } },

            // 大写字母 A - Z
            {'A', new List<string> { "2:80", "2:40", "2:20", "2:1", "1:1", "1:20", "1:40", "2:10" } },
            {'B', new List<string> { "2:80", "2:40", "2:20", "1:1", "0:10", "0:40", "0:80", "1:10", "1:20", "1:40" } },
            {'C', new List<string> { "2:40", "2:80", "1:10", "0:80", "0:40" } },
            {'D', new List<string> { "2:20", "1:1", "0:10", "0:40", "0:80", "1:10", "2:80" } },
            {'E', new List<string> { "2:40", "2:80", "1:10", "1:40", "0:20", "0:40", "0:80" } },
            {'F', new List<string> { "2:40", "2:80", "1:10", "1:40", "1:20" } },
            {'G', new List<string> { "2:40", "2:80", "1:10", "1:40", "0:20", "0:40", "0:80", "1:1", "2:20" } },
            {'H', new List<string> { "2:80", "2:10", "1:10", "1:40", "1:20", "2:20" } },
            {'I', new List<string> { "2:40", "0:40" } },
            {'J', new List<string> { "2:20", "1:1", "0:10", "0:40", "0:80" } },
            {'K', new List<string> { "2:80", "2:10", "1:10", "1:40", "1:20", "1:1" } },
            {'L', new List<string> { "2:80", "1:10", "0:80", "0:40" } },
            {'M', new List<string> { "2:80", "2:10", "1:10", "1:80", "1:20", "1:1", "2:20" } },
            {'N', new List<string> { "2:80", "2:10", "1:10", "1:20", "2:20" } },
            {'O', new List<string> { "2:80", "2:40", "2:20", "2:1", "1:1", "0:10", "0:40", "0:80", "1:10", "2:10" } },
            {'P', new List<string> { "2:80", "2:40", "2:20", "2:1", "1:1", "1:20", "1:40", "2:10" } },
            {'Q', new List<string> { "2:80", "2:40", "2:20", "2:1", "1:1", "0:10", "0:40", "0:80", "1:10", "2:10", "1:2" } },
            {'R', new List<string> { "2:80", "2:40", "2:20", "2:1", "1:1", "1:20", "1:40", "2:10", "1:10" } },
            {'S', new List<string> { "2:40", "2:80", "1:10", "1:40", "1:20", "0:10", "0:40" } },
            {'T', new List<string> { "2:40", "0:40" } },
            {'U', new List<string> { "2:80", "1:10", "0:80", "0:10", "1:1", "2:20" } },
            {'V', new List<string> { "2:80", "1:10", "0:80", "0:10", "1:1" } },
            {'W', new List<string> { "2:80", "1:10", "0:80", "0:20", "0:10", "1:1", "2:20" } },
            {'X', new List<string> { "2:80", "2:10", "1:10", "1:20", "2:20", "1:1", "0:10" } },
            {'Y', new List<string> { "2:80", "2:10", "1:10", "1:20", "0:10", "1:1" } },
            {'Z', new List<string> { "2:40", "2:20", "1:20", "0:20", "0:40", "0:80" } },

            // 小写字母 a - z
            {'a', new List<string> { "1:40", "1:20", "0:10", "0:40", "0:80", "1:10" } },
            {'b', new List<string> { "2:80", "1:10", "1:40", "1:20", "0:10", "0:40", "0:80" } },
            {'c', new List<string> { "1:40", "0:20", "0:40", "0:80" } },
            {'d', new List<string> { "2:20", "1:1", "1:20", "0:10", "0:40", "0:80", "1:10" } },
            {'e', new List<string> { "1:40", "1:20", "0:10", "0:40", "0:80", "1:10", "2:40" } },
            {'f', new List<string> { "2:40", "2:80", "1:10", "1:40", "1:20" } },
            {'g', new List<string> { "2:20", "1:1", "1:20", "0:10", "0:40", "0:80", "1:10", "2:10" } },
            {'h', new List<string> { "2:80", "1:10", "1:40", "1:20" } },
            {'i', new List<string> { "1:40", "0:40" } },
            {'j', new List<string> { "1:1", "0:10", "0:40", "0:80" } },
            {'k', new List<string> { "2:80", "1:10", "1:40", "1:20", "1:1" } },
            {'l', new List<string> { "2:80", "1:10", "0:80", "0:40" } },
            {'m', new List<string> { "1:80", "1:40", "1:2", "1:20" } },
            {'n', new List<string> { "1:10", "1:40", "1:20" } },
            {'o', new List<string> { "1:40", "1:20", "0:10", "0:40", "0:80", "1:10" } },
            {'p', new List<string> { "2:40", "2:80", "1:10", "1:40", "1:20" } },
            {'q', new List<string> { "2:20", "1:1", "1:20", "2:10" } },
            {'r', new List<string> { "1:40", "1:20" } },
            {'s', new List<string> { "1:10", "1:40", "1:20", "0:10", "0:40" } },
            {'t', new List<string> { "1:40", "0:20", "0:40", "0:80", "2:40" } },
            {'u', new List<string> { "1:10", "0:80", "0:10", "1:1" } },
            {'v', new List<string> { "1:10", "0:80", "0:10", "1:1" } },
            {'w', new List<string> { "1:10", "0:80", "0:20", "0:10", "1:1" } },
            {'x', new List<string> { "1:10", "1:20", "1:1", "0:10" } },
            {'y', new List<string> { "1:10", "1:20", "0:10", "1:1", "2:10" } },
            {'z', new List<string> { "1:40", "1:20", "0:20", "0:40" } }
        };

        public MainWindow()
        {
            InitializeComponent();
            GenerateSegmentCodeTable();
        }

        private void Line_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (sender is Line line)
            {
                string tag = line.Tag.ToString();
                if (line.Stroke == Brushes.Black)
                {
                    line.Stroke = Brushes.Red;
                    selectedSegments.Add(tag);
                }
                else
                {
                    line.Stroke = Brushes.Black;
                    selectedSegments.Remove(tag);
                }
            }
        }

        private void GetCodeButton_Click(object sender, RoutedEventArgs e)
        {
            UInt32 codes = 0;
            foreach (string segment in selectedSegments)
            {
                if (encodingRules.ContainsKey(segment))
                {
                    codes |= encodingRules[segment];
                }
            }
            ResultTextBox.AppendText(Convert.ToString(codes, 16)+"\n");
        }

        private void GenerateSegmentCodeTable()
        {
            string table = "段码表：\n";
            foreach (var pair in charSegments)
            {
                char character = pair.Key;
                List<string> segments = pair.Value;
                UInt32 code = 0;
                foreach (string segment in segments)
                {
                    if (encodingRules.ContainsKey(segment))
                    {
                        code |= encodingRules[segment];
                    }
                }
                table += $"{character}: 0x{code:X}\n";
            }
            ResultTextBox.Text = table;
        }
        private void DisplayHexCode(uint hexCode)
        {
            // 清空之前选中的线段
            selectedSegments.Clear();

            // 遍历编码规则，检查每个线段对应的编码是否在 hexCode 中被设置
            foreach (var pair in encodingRules)
            {
                string segmentTag = pair.Key;
                uint segmentCode = pair.Value;

                if ((hexCode & segmentCode) != 0)
                {
                    // 如果该线段对应的编码在 hexCode 中被设置，则点亮该线段
                    selectedSegments.Add(segmentTag);
                    // 找到对应的 Line 控件并将其颜色设置为红色
                    foreach (UIElement element in DigitCanvas.Children)
                    {
                        if (element is Line line && line.Tag.ToString() == segmentTag)
                        {
                            line.Stroke = Brushes.Red;
                        }
                    }
                }
                else
                {
                    // 如果该线段对应的编码在 hexCode 中未被设置，则熄灭该线段
                    foreach (UIElement element in DigitCanvas.Children)
                    {
                        if (element is Line line && line.Tag.ToString() == segmentTag)
                        {
                            line.Stroke = Brushes.Black;
                        }
                    }
                }
            }

            // 在 ResultTextBox 中显示编码结果
            ResultTextBox.Text = $"当前显示编码: 0x{hexCode:X}";
        }

        private void SetCodeButton_Click(object sender, RoutedEventArgs e)
        {

            DisplayHexCode(Convert.ToUInt32( tb_content.Text,16));
        }
    }
}