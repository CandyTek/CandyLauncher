﻿using HakeQuick.Abstraction.MVVM;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Media.Imaging;

namespace HakeQuick.Abstraction.Action
{
    public enum ActionPriority
    {
        Topmost = 0,
        VeryHigh = 1,
        High = 2,
        Normal = 3,
        Low = 4,
        VeryLow = 5
    }

    public abstract class ActionBase : ViewModelBase
    {
        /// <summary>
        /// 图标
        /// </summary>
        private BitmapImage _icon;
        public BitmapImage Icon
        {
            get { return _icon; }
            protected set { _icon = value; NotifyPropertyChanged(nameof(Icon)); }
        }
        /// <summary>
        /// 标题
        /// </summary>
        private string _title;
        public string Title
        {
            get { return _title; }
            protected set { _title = value; NotifyPropertyChanged(nameof(Title)); }
        }

        /// <summary>
        /// 副标题
        /// </summary>
        private string _subtitle;
        public string Subtitle
        {
            get { return _subtitle; }
            protected set { _subtitle = value; NotifyPropertyChanged(nameof(Subtitle)); }
        }

        /// <summary>
        /// 可执行文件
        /// </summary>
        private bool _executable;
        public bool IsExecutable
        {
            get { return _executable; }
            protected set { _executable = value; NotifyPropertyChanged(nameof(IsExecutable)); }
        }

        // Create member called Invoke
    }

    public static class ActionBaseExtensions
    {

    }
}
