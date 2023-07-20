﻿using HakeQuick.Implementation.Services.Window;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using HakeQuick.Abstraction.Action;
using HakeQuick.Abstraction.Services;
using System.Collections.ObjectModel;
using System.Windows.Forms.Integration;
using System.Windows.Interop;
using HakeQuick.Helpers;
using System.Runtime.InteropServices;

namespace HakeQuick
{
    public partial class DefaultWindow : Window, IQuickWindow
    {
        public static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);

        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint uFlags);

        public string RawInput
        {
            get { return textbox_input.Text; }
            set
            {
                textbox_input.Text = value;
            }
        }

        public event EventHandler<TextUpdatedEventArgs> TextChanged;
        public event EventHandler<ExecutionRequestedEventArgs> ExecutionRequested;
        public event EventHandler VisibleChanged;


        private ObservableCollection<ActionBase> m_actions = null;
        private IntPtr hwnd;

        public DefaultWindow()
        {
            InitializeComponent();
            Loaded += OnLoaded;
        }

       

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            hwnd = new WindowInteropHelper(this).Handle;
            GlassHelper.EnableAero(hwnd);

            textbox_input.TextChanged += OnTextChanged;
            IsVisibleChanged += OnIsVisibleChanged;
            list_actions.PreviewMouseLeftButtonDown += OnListPreviewLeftMouseButtonDown;
            Deactivated += OnDeactived;
            PreviewKeyDown += OnPreviewKeyDown;
            ElementHost.EnableModelessKeyboardInterop(this);
        }

        private void OnPreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Handled) return;
            if (m_actions == null || m_actions.Count <= 0 || IsVisible == false) return;
            if (e.Key == Key.Down)
            {
                MoveToNextAction();
                e.Handled = true;
            }
            else if (e.Key == Key.Up)
            {
                MoveToPreviousAction();
                e.Handled = true;
            }
            else if (e.Key == Key.Enter)
            {
                ActionBase action = list_actions.SelectedItem as ActionBase;
                if (!action.IsExecutable) { e.Handled = true; return; }
                ExecutionRequested?.Invoke(this, new ExecutionRequestedEventArgs(action));
                e.Handled = true;
            }
        }

        private void OnDeactived(object sender, EventArgs e)
        {
            HideWindow();
        }

        private void OnListPreviewLeftMouseButtonDown(object sender, MouseButtonEventArgs e)
        {
            e.Handled = true;
        }

        private void OnTextChanged(object sender, TextChangedEventArgs e)
        {
            if (e.Handled)
                return;

            TextChanged?.Invoke(this, new TextUpdatedEventArgs(textbox_input.Text));
        }

        private void OnIsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            VisibleChanged?.Invoke(this, null);
        }

        public void ClearInput()
        {
            textbox_input.Text = "";
            TextChanged?.Invoke(this, new TextUpdatedEventArgs(textbox_input.Text));
        }

        public void HideWindow()
        {
            if (IsVisible == false)
                return;
            this.Hide();
        }

        public void SetActions(ObservableCollection<ActionBase> actions)
        {
            list_actions.ItemsSource = actions;
            m_actions = actions;
        }

        /// <summary>
        /// 弹出主窗口，并调整布局、位置
        /// </summary>
        /// <param name="context"></param>
        public void ShowWindow(IProgramContext context)
        {
            if (IsVisible == true)
                return;
            // 获取所有屏幕信息
            System.Windows.Forms.Screen[] screens = System.Windows.Forms.Screen.AllScreens;
            
            if (true)
            {
                // 固定屏幕中间
                Left = (screens[0].Bounds.Width / 2 - Width /2);
                Top = (screens[0].Bounds.Height * 0.2);
            }
            else
            {
                // 获取每个屏幕的DPI 值
                List<uint> dpis = ScreenHelpers.GetScreenDpis(screens);
                // 传入对象的窗口位置信息
                RECT position = context.WindowPosition;
                //double ttop = position.Top + 50;
                // 根据窗口位置，再往下100
                double ttop = position.Top + 200;
                double windowWidth = ActualWidth;
                if (windowWidth <= 0)
                    windowWidth = Width;
                // 计算缩放比例 
                uint scale = dpis[0] / 96;
                windowWidth *= scale;
                double halfwidthdiff = ((position.Right - position.Left) - windowWidth) / 2;
                double tleft = position.Left + halfwidthdiff;
                if (tleft < 0 && tleft + windowWidth > 0)
                    tleft = -(windowWidth + 50);
                if (ttop < 200)
                    ttop = 200;
                // 得到缩放后的位置
                Left = tleft / scale;
                Top = ttop / scale;
            }
            // 显示窗口
            Show();
            // 激活窗口
            Activate();
            // 使聚焦输入框
            textbox_input.Focus();
            // 置顶显示
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 3);
        }

        private void MoveToNextAction()
        {
            int index = list_actions.SelectedIndex;
            if (index == -1)
            {
                for (int i = 0; i < m_actions.Count; i++)
                {
                    if (m_actions[i].IsExecutable)
                    {
                        list_actions.SelectedIndex = i;
                        list_actions.ScrollIntoView(list_actions.SelectedItem);
                        break;
                    }
                }
            }
            else
            {
                int oindex = index;
                do
                {
                    index++;
                    if (index < 0)
                        index = m_actions.Count - 1;
                    if (index >= m_actions.Count)
                        index = 0;
                    if (index == oindex)
                        break;
                } while (!m_actions[index].IsExecutable);
                if (m_actions[index].IsExecutable)
                {
                    list_actions.SelectedIndex = index;
                    list_actions.ScrollIntoView(list_actions.SelectedItem);
                }
                else
                    list_actions.SelectedIndex = -1;
            }
        }
        private void MoveToPreviousAction()
        {
            int index = list_actions.SelectedIndex;
            if (index == -1)
            {
                for (int i = m_actions.Count - 1; i >= 0; i--)
                {
                    if (m_actions[i].IsExecutable)
                    {
                        list_actions.SelectedIndex = i;
                        list_actions.ScrollIntoView(list_actions.SelectedItem);
                        break;
                    }
                }
            }
            else
            {
                int oindex = index;
                do
                {
                    index--;
                    if (index < 0)
                        index = m_actions.Count - 1;
                    if (index >= m_actions.Count)
                        index = 0;
                    if (index == oindex)
                        break;
                } while (!m_actions[index].IsExecutable);
                if (m_actions[index].IsExecutable)
                {
                    list_actions.SelectedIndex = index;
                    list_actions.ScrollIntoView(list_actions.SelectedItem);
                }
                else
                    list_actions.SelectedIndex = -1;
            }
        }

        public void OnActionUpdateCompleted()
        {
            list_actions.SelectedIndex = -1;
            MoveToNextAction();
        }


        public void SetKeyDown(System.Windows.Input.KeyEventHandler keyEventHandler)
        {
            this.KeyDown += keyEventHandler;
            //throw new NotImplementedException();
        }
    }
}
