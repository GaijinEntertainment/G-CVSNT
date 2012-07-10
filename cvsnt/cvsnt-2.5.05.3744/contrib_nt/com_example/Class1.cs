using System;
using System.Windows.Forms;
using System.Diagnostics;
using CVSNT;

namespace cvscom
{
	public class Class1 : CVSNT.ICvsInfo6
	{
		#region ICvsInfo6 Members

		public short get_template(string directory, out string template_ptr)
		{
			// TODO:  Add Class1.get_template implementation
			Debug.WriteLine("get_template "+directory);
			template_ptr = null;
			return 0;
		}

		public short premodule(string module_name)
		{
			// TODO:  Add Class1.premodule implementation
			Debug.WriteLine("premodule "+module_name);
			return 0;
		}

		public short postcommand(string directory)
		{
			// TODO:  Add Class1.postcommand implementation
			Debug.WriteLine("postcommand "+directory);
			return 0;
		}

		public short precommand(Array argv)
		{
			// TODO:  Add Class1.precommand implementation
			Debug.WriteLine("precommand" + Convert.ToString(argv.GetUpperBound(0)+1) + " arguments");
			return 0;
		}

		public short verifymsg(string directory, string filename)
		{
			// TODO:  Add Class1.verifymsg implementation
			Debug.WriteLine("verifymsg "+directory+" "+filename);
			return 0;
		}

		public short precommit(Array name_list, string message, string directory)
		{
			// TODO:  Add Class1.precommit implementation
			Debug.WriteLine("precommit "+directory);
			return 0;
		}

		public short parse_keyword(string keyword, string directory, string file, string branch, string author, string printable_date, string rcs_date, string locker, string state, string name, string version, string bugid, string commitid, Array props, out string value)
		{
			// TODO:  Add Class1.parse_keyword implementation
			Debug.WriteLine("parse_keyword "+keyword+" "+directory+" "+file);
			value = null;
			return 0;
		}

		public short init(string command, string date, string hostname, string virtual_repository, string physical_repository, string sessionid, string editor, Array uservar, Array userval, string client_version)
		{
			// TODO:  Add Class1.init implementation
			Debug.WriteLine("init "+command+" "+date+" "+hostname);
			return 0;
		}

		public short notify(string message, string bugid, string directory, string notify_user, string tag, string type, string file)
		{
			// TODO:  Add Class1.notify implementation
			Debug.WriteLine("notify "+message+" "+notify_user);
			return 0;
		}

		public short history(sbyte type, string workdir, string revs, string name, string bugid, string message)
		{
			// TODO:  Add Class1.history implementation
			Debug.WriteLine("history "+type+" "+workdir+" "+revs+" "+name+" "+bugid+" "+message);
			return 0;
		}

		short CVSNT.ICvsInfo6.history(sbyte type, string workdir, string revs, string name)
		{
			return this.history(type,workdir,revs,name,null,null);
		}

		public short loginfo(string message, string status, string directory, Array change_list)
		{
			// TODO:  Add Class1.loginfo implementation
			Debug.WriteLine("loginfo "+directory);
			return 0;
		}

		public short prercsdiff(string file, string directory, string oldfile, string newfile, string type, string options, string oldversion, string newversion, int added, int removed)
		{
			// TODO:  Add Class1.prercsdiff implementation
			Debug.WriteLine("prercsdiff "+file+" "+directory);
			return 0;
		}

		public short pretag(string message, string directory, Array name_list, Array version_list, sbyte tag_type, string action, string tag)
		{
			// TODO:  Add Class1.pretag implementation
			Debug.WriteLine("pretag "+directory);
			return 0;
		}

		public short postcommit(string directory)
		{
			// TODO:  Add Class1.postcommit implementation
			Debug.WriteLine("postcommit "+directory);
			return 0;
		}

		public short close()
		{
			// TODO:  Add Class1.close implementation
			Debug.WriteLine("close");
			return 0;
		}

		public short rcsdiff(string file, string directory, string oldfile, string newfile, string diff, string type, string options, string oldversion, string newversion, int added, int removed)
		{
			// TODO:  Add Class1.rcsdiff implementation
			Debug.WriteLine("rcsdiff "+file+" "+directory);
			return 0;
		}

		public short postmodule(string module_name)
		{
			// TODO:  Add Class1.postmodule implementation
			Debug.WriteLine("postmodule "+module_name);
			return 0;
		}

		#endregion

		#region ICvsInfo5 Members

		short CVSNT.ICvsInfo5.history(sbyte type, string workdir, string revs, string name)
		{
			return this.history(type,workdir,revs,name,null,null);
		}

		#endregion

		#region ICvsInfo4 Members

		short CVSNT.ICvsInfo4.history(sbyte type, string workdir, string revs, string name)
		{
			return this.history(type,workdir,revs,name,null,null);
		}

		#endregion

	}
}
